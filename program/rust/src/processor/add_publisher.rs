use {
    crate::{
        accounts::{
            PriceAccount,
            PriceComponent,
            PythAccount,
        },
        c_oracle_header::PC_NUM_COMP,
        deserialize::{
            load,
            load_checked,
        },
        instruction::AddPublisherArgs,
        utils::{
            check_permissioned_funding_account,
            check_valid_funding_account,
            pyth_assert,
            try_convert,
        },
        OracleError,
    },
    bytemuck::bytes_of_mut,
    solana_program::{
        account_info::AccountInfo,
        entrypoint::ProgramResult,
        program_error::ProgramError,
        program_memory::{
            sol_memcmp,
            sol_memset,
        },
        pubkey::Pubkey,
    },
    std::mem::size_of,
};

/// Add publisher to symbol account
// account[0] funding account       [signer writable]
// account[1] price account         [signer writable]
pub fn add_publisher(
    program_id: &Pubkey,
    accounts: &[AccountInfo],
    instruction_data: &[u8],
) -> ProgramResult {
    let cmd_args = load::<AddPublisherArgs>(instruction_data)?;

    pyth_assert(
        instruction_data.len() == size_of::<AddPublisherArgs>(),
        ProgramError::InvalidArgument,
    )?;

    let (funding_account, price_account, permissions_account) = match accounts {
        [x, y, p] => Ok((x, y, p)),
        _ => Err(OracleError::InvalidNumberOfAccounts),
    }?;

    check_valid_funding_account(funding_account)?;
    check_permissioned_funding_account(
        program_id,
        price_account,
        funding_account,
        permissions_account,
        &cmd_args.header,
    )?;


    let mut price_data = load_checked::<PriceAccount>(price_account, cmd_args.header.version)?;

    // Use the call with the default pubkey (000..) as a trigger to sort the publishers as a
    // migration step from unsorted list to sorted list.
    if cmd_args.publisher == Pubkey::default() {
        let num_comps = try_convert::<u32, usize>(price_data.num_)?;
        sort_price_comps(&mut price_data.comp_, num_comps)?;
        return Ok(());
    }

    if price_data.num_ >= PC_NUM_COMP {
        return Err(ProgramError::InvalidArgument);
    }

    for i in 0..(try_convert::<u32, usize>(price_data.num_)?) {
        if cmd_args.publisher == price_data.comp_[i].pub_ {
            return Err(ProgramError::InvalidArgument);
        }
    }

    let current_index: usize = try_convert(price_data.num_)?;
    sol_memset(
        bytes_of_mut(&mut price_data.comp_[current_index]),
        0,
        size_of::<PriceComponent>(),
    );
    price_data.comp_[current_index].pub_ = cmd_args.publisher;
    price_data.num_ += 1;

    // Sort the publishers in the list
    {
        let num_comps = try_convert::<u32, usize>(price_data.num_)?;
        sort_price_comps(&mut price_data.comp_, num_comps)?;
    }

    price_data.header.size = try_convert::<_, u32>(PriceAccount::INITIAL_SIZE)?;
    Ok(())
}

/// A copy of rust slice/sort.rs heapsort implementation which is small and fast. We couldn't use
/// the sort directly because it was only accessible behind a unstable feature flag at the time of
/// writing this code.
fn heapsort(v: &mut [(Pubkey, usize)]) {
    // This binary heap respects the invariant `parent >= child`.
    let sift_down = |v: &mut [(Pubkey, usize)], mut node: usize| {
        loop {
            // Children of `node`.
            let mut child = 2 * node + 1;
            if child >= v.len() {
                break;
            }

            // Choose the greater child.
            if child + 1 < v.len()
                && sol_memcmp(v[child].0.as_ref(), v[child + 1].0.as_ref(), 32) < 0
            {
                child += 1;
            }

            // Stop if the invariant holds at `node`.
            if sol_memcmp(v[node].0.as_ref(), v[child].0.as_ref(), 32) >= 0 {
                break;
            }

            // Swap `node` with the greater child, move one step down, and continue sifting.
            v.swap(node, child);
            node = child;
        }
    };

    // Build the heap in linear time.
    for i in (0..v.len() / 2).rev() {
        sift_down(v, i);
    }

    // Pop maximal elements from the heap.
    for i in (1..v.len()).rev() {
        v.swap(0, i);
        sift_down(&mut v[..i], 0);
    }
}

/// Sort the publishers price component list in place by performing minimal swaps.
/// This code is inspired by the sort_by_cached_key implementation in the Rust stdlib.
/// The rust stdlib implementation is not used because it uses a fast sort variant that has
/// a large code size.
///
/// num_publishers is the number of publishers in the list that should be sorted. It is explicitly
/// passed to avoid callers mistake of passing the full slice which may contain uninitialized values.
fn sort_price_comps(comps: &mut [PriceComponent], num_comps: usize) -> Result<(), ProgramError> {
    let comps = comps
        .get_mut(..num_comps)
        .ok_or(ProgramError::InvalidArgument)?;

    // Publishers are likely sorted in ascending order but
    // heapsorts creates a max-heap so we reverse the order
    // of the keys to make the heapify step faster.
    let mut keys = comps
        .iter()
        .enumerate()
        .map(|(i, x)| (x.pub_, i))
        .rev()
        .collect::<Vec<_>>();

    heapsort(&mut keys);

    for i in 0..num_comps {
        // We know that the publisher with key[i].0 should be at index i in the sorted array and
        // want to swap them in-place in O(n). Normally, the publisher at key[i].0 should be at
        // key[i].1 but if it is swapped, we need to find the correct index by following the chain
        // of swaps.
        let mut index = keys[i].1;

        while index < i {
            index = keys[index].1;
        }
        // Setting the final index here is important to make the code linear as we won't
        // loop over from i to index again when we reach i again.
        keys[i].1 = index;
        comps.swap(i, index);
    }

    Ok(())
}

#[cfg(test)]
mod test {
    use {
        super::*,
        quickcheck_macros::quickcheck,
    };

    #[quickcheck]
    pub fn test_sort_price_comps(mut comps: Vec<PriceComponent>) {
        let mut rust_std_sorted_comps = comps.clone();
        rust_std_sorted_comps.sort_by_key(|x| x.pub_);

        let num_comps = comps.len();
        assert_eq!(
            sort_price_comps(&mut comps, num_comps + 1),
            Err(ProgramError::InvalidArgument)
        );

        assert_eq!(sort_price_comps(&mut comps, num_comps), Ok(()));
        assert_eq!(comps, rust_std_sorted_comps);
    }

    #[quickcheck]
    pub fn test_sort_price_comps_smaller_slice(
        mut comps: Vec<PriceComponent>,
        mut num_comps: usize,
    ) {
        num_comps = if comps.is_empty() {
            0
        } else {
            num_comps % comps.len()
        };

        let mut rust_std_sorted_comps = comps.get(..num_comps).unwrap().to_vec();
        rust_std_sorted_comps.sort_by_key(|x| x.pub_);


        assert_eq!(sort_price_comps(&mut comps, num_comps), Ok(()));
        assert_eq!(comps.get(..num_comps).unwrap(), rust_std_sorted_comps);
    }
}
