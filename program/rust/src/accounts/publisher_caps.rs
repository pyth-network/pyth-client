use {
    super::{
        AccountHeader,
        PythAccount,
    },
    crate::c_oracle_header::{
        PC_ACCTYPE_SCORE,
        PC_MAX_PUBLISHERS,
        PC_MAX_SYMBOLS,
        PC_MAX_SYMBOLS_64,
    },
    bytemuck::{
        Pod,
        Zeroable,
    },
    solana_program::{
        program_error::ProgramError,
        pubkey::Pubkey,
    },
    std::cmp::max,
};

#[repr(C)]
#[cfg_attr(test, derive(Debug))]
#[derive(Copy, Clone, Pod, Zeroable)]

/// This account is part of Community Integrity Pool (CIP) project.
/// It is used to store the caps of the publishers which will be sent
/// to the `integrity_pool` program on mainnet to calculate the rewards.
pub struct PublisherCapsAccount {
    pub header:         AccountHeader,
    pub num_publishers: usize,
    pub num_symbols:    usize,
    // Z is a constant used to normalize the caps
    pub z:              u64,
    // M is a constant showing the target stake per symbol
    pub m:              u64,

    // array[x][y] is a u64 whose bits represent if publisher x publishes symbols 64*y to 64*(y+1) - 1
    pub publisher_permissions: [[u64; PC_MAX_SYMBOLS_64 as usize]; PC_MAX_PUBLISHERS as usize],
    pub caps:                  [u64; PC_MAX_PUBLISHERS as usize],
    pub publishers:            [Pubkey; PC_MAX_PUBLISHERS as usize],
    pub prices:                [Pubkey; PC_MAX_SYMBOLS as usize],
}

impl PublisherCapsAccount {
    fn get_publisher_permission(&self, x: usize, y: usize) -> bool {
        (self.publisher_permissions[x][y / 64] >> (y % 64)) & 1 == 1
    }

    fn set_publisher_permission(&mut self, x: usize, y: usize, value: bool) {
        if value {
            self.publisher_permissions[x][y / 64] |= 1 << (y % 64);
        } else {
            self.publisher_permissions[x][y / 64] &= !(1 << (y % 64));
        }
    }

    pub fn add_publisher(
        &mut self,
        publisher: Pubkey,
        price_account: Pubkey,
    ) -> Result<(), ProgramError> {
        let publisher_index = self
            .publishers
            .iter()
            .position(|&x| x == publisher)
            .unwrap_or(self.num_publishers);

        if publisher_index == self.num_publishers {
            if self.num_publishers == PC_MAX_PUBLISHERS as usize {
                return Err(ProgramError::AccountDataTooSmall);
            }

            self.publishers[self.num_publishers] = publisher;
            self.num_publishers += 1;
        }

        let symbol_index = self
            .prices
            .iter()
            .position(|&x| x == price_account)
            .ok_or(ProgramError::InvalidArgument)?;

        self.set_publisher_permission(publisher_index, symbol_index, true);

        self.calculate_caps()
    }

    pub fn del_publisher(
        &mut self,
        publisher: Pubkey,
        price_account: Pubkey,
    ) -> Result<(), ProgramError> {
        let publisher_index = self
            .publishers
            .iter()
            .position(|&x| x == publisher)
            .ok_or(ProgramError::InvalidArgument)?;

        let price_index = self
            .prices
            .iter()
            .position(|&x| x == price_account)
            .ok_or(ProgramError::InvalidArgument)?;

        self.set_publisher_permission(publisher_index, price_index, false);
        self.calculate_caps()
    }

    pub fn add_price(&mut self, symbol: Pubkey) -> Result<(), ProgramError> {
        let price_index = self
            .prices
            .iter()
            .position(|&x| x == symbol)
            .unwrap_or(self.num_symbols);

        if price_index == self.num_symbols {
            if self.num_symbols == PC_MAX_SYMBOLS as usize {
                return Err(ProgramError::AccountDataTooSmall);
            }

            self.prices[self.num_symbols] = symbol;
            self.num_symbols += 1;
        }

        Ok(())
    }

    pub fn del_price(&mut self, symbol: Pubkey) -> Result<(), ProgramError> {
        let price_index = self
            .prices
            .iter()
            .position(|&x| x == symbol)
            .ok_or(ProgramError::InvalidArgument)?;

        // update symbol list
        self.prices[price_index] = self.prices[self.num_symbols - 1];
        self.prices[self.num_symbols - 1] = Pubkey::default();

        // update publisher permissions
        for i in 0..self.num_publishers {
            let value = self.get_publisher_permission(i, self.num_symbols - 1);
            self.set_publisher_permission(i, price_index, value)
        }

        self.num_symbols -= 1;
        self.calculate_caps()
    }


    pub fn calculate_caps(&mut self) -> Result<(), ProgramError> {
        let price_weights: Vec<u64> = self
            .prices
            .iter()
            .enumerate()
            .map(|(j, _)| {
                let weight = self
                    .publisher_permissions
                    .iter()
                    .enumerate()
                    .filter(|(i, _)| self.get_publisher_permission(*i, j))
                    .count() as u64;
                max(weight, self.z)
            })
            .collect();

        for i in 0..self.num_publishers {
            self.caps[i] = self
                .prices
                .iter()
                .enumerate()
                .filter(|(j, _)| self.get_publisher_permission(i, *j))
                .map(|(j, _)| self.m * 1_000_000_000_u64 / price_weights[j])
                .sum();
        }
        Ok(())
    }

    pub fn get_caps_message(&self) -> Vec<Vec<u8>> {
        self.publishers
            .iter()
            .enumerate()
            .map(|(i, pubkey)| {
                pubkey
                    .to_bytes()
                    .into_iter()
                    .chain(self.caps[i].to_le_bytes().into_iter())
                    .collect()
            })
            .collect()
    }
}

impl PythAccount for PublisherCapsAccount {
    const ACCOUNT_TYPE: u32 = PC_ACCTYPE_SCORE;
    // Calculate the initial size of the account
    const INITIAL_SIZE: u32 = 75824;
}


#[cfg(test)]
mod tests {
    use {
        super::*,
        crate::c_oracle_header::{
            PC_ACCTYPE_SCORE,
            PC_MAGIC,
            PC_VERSION,
        },
        quickcheck::Arbitrary,
        quickcheck_macros::quickcheck,
        solana_program::pubkey::Pubkey,
        std::mem::size_of,
    };

    fn arbitrary_pubkey(g: &mut quickcheck::Gen) -> Pubkey {
        let mut key = [0u8; 32];
        key.iter_mut().for_each(|item| *item = u8::arbitrary(g));
        Pubkey::new_from_array(key)
    }

    impl Arbitrary for PublisherCapsAccount {
        fn arbitrary(g: &mut quickcheck::Gen) -> Self {
            let mut account = PublisherCapsAccount {
                header:                AccountHeader {
                    magic_number: PC_MAGIC,
                    account_type: PC_ACCTYPE_SCORE,
                    version:      PC_VERSION,
                    size:         size_of::<PublisherCapsAccount>() as u32,
                },
                num_publishers:        usize::arbitrary(g) % (PC_MAX_PUBLISHERS - 10) as usize + 10,
                num_symbols:           usize::arbitrary(g) % (PC_MAX_SYMBOLS - 10) as usize + 10,
                z:                     u64::arbitrary(g) % 5 + 2,
                m:                     u64::arbitrary(g) % 1000 + 1000,
                publisher_permissions: [[u64::arbitrary(g); PC_MAX_SYMBOLS_64 as usize];
                    PC_MAX_PUBLISHERS as usize],
                caps:                  [0; PC_MAX_PUBLISHERS as usize],
                publishers:            [arbitrary_pubkey(g); PC_MAX_PUBLISHERS as usize],
                prices:                [arbitrary_pubkey(g); PC_MAX_SYMBOLS as usize],
            };

            account.calculate_caps().unwrap();
            account
        }
    }

    fn get_empty_account() -> PublisherCapsAccount {
        PublisherCapsAccount {
            header:                AccountHeader {
                magic_number: PC_MAGIC,
                account_type: PC_ACCTYPE_SCORE,
                version:      PC_VERSION,
                size:         size_of::<PublisherCapsAccount>() as u32,
            },
            num_publishers:        0,
            num_symbols:           0,
            z:                     2,
            m:                     1000,
            publisher_permissions: [[0; PC_MAX_SYMBOLS_64 as usize]; PC_MAX_PUBLISHERS as usize],
            caps:                  [0; PC_MAX_PUBLISHERS as usize],
            publishers:            [Pubkey::default(); PC_MAX_PUBLISHERS as usize],
            prices:                [Pubkey::default(); PC_MAX_SYMBOLS as usize],
        }
    }

    #[test]
    fn test_size_of_publisher_caps_account() {
        let account = get_empty_account();
        assert_eq!(
            PublisherCapsAccount::INITIAL_SIZE as usize,
            size_of::<PublisherCapsAccount>()
        );
        assert_eq!(
            account.header.size as usize,
            size_of::<PublisherCapsAccount>()
        );
    }

    #[test]
    fn test_publisher_caps_account() {
        let mut account = get_empty_account();

        let publisher1 = Pubkey::new_unique();
        let publisher2 = Pubkey::new_unique();
        let publisher3 = Pubkey::new_unique();
        let symbol1 = Pubkey::new_unique();
        let symbol2 = Pubkey::new_unique();

        account.add_price(symbol1).unwrap();
        account.add_price(symbol2).unwrap();

        account.add_publisher(publisher1, symbol1).unwrap();
        assert_eq!(account.caps[..2], [account.m * 500_000_000, 0]);

        account.add_publisher(publisher1, symbol2).unwrap();
        assert!(account.get_publisher_permission(0, 0));
        assert!(account.get_publisher_permission(0, 1));
        assert_eq!(account.caps[..2], [account.m * 1_000_000_000, 0]);


        account.add_publisher(publisher2, symbol1).unwrap();
        assert_eq!(
            account.caps[..2],
            [account.m * 1_000_000_000, account.m * 500_000_000]
        );

        account.add_publisher(publisher2, symbol2).unwrap();
        assert!(account.get_publisher_permission(1, 0));
        assert!(account.get_publisher_permission(1, 1));
        assert_eq!(
            account.caps[..2],
            [account.m * 1_000_000_000, account.m * 1_000_000_000]
        );

        account.add_publisher(publisher3, symbol1).unwrap();
        assert_eq!(
            account.caps[..3],
            [833_333_333_333, 833_333_333_333, 333333333333]
        );

        account.del_publisher(publisher1, symbol1).unwrap();
        assert_eq!(account.num_publishers, 3);
        assert_eq!(account.num_symbols, 2);
        assert!(!account.get_publisher_permission(0, 0));
        assert!(account.get_publisher_permission(0, 1));
        assert!(account.get_publisher_permission(1, 0));
        assert!(account.get_publisher_permission(1, 1));

        account.del_publisher(publisher1, symbol2).unwrap();
        assert_eq!(account.num_publishers, 3);
        assert_eq!(account.num_symbols, 2);
        assert!(!account.get_publisher_permission(0, 0));
        assert!(!account.get_publisher_permission(0, 1));
        assert!(account.get_publisher_permission(1, 0));
        assert!(account.get_publisher_permission(1, 1));

        account.del_publisher(publisher2, symbol1).unwrap();
        assert_eq!(account.num_publishers, 3);
        assert_eq!(account.num_symbols, 2);
        assert!(!account.get_publisher_permission(0, 0));
        assert!(!account.get_publisher_permission(0, 1));
        assert!(!account.get_publisher_permission(1, 0));
        assert!(account.get_publisher_permission(1, 1));

        account.del_publisher(publisher2, symbol2).unwrap();
        assert_eq!(account.num_publishers, 3);
        assert_eq!(account.num_symbols, 2);
        assert!(!account.get_publisher_permission(0, 0));
        assert!(!account.get_publisher_permission(0, 1));
        assert!(!account.get_publisher_permission(1, 0));
        assert!(!account.get_publisher_permission(1, 1));

        assert_eq!(account.caps[..3], [0, 0, 500000000000]);
    }

    #[allow(clippy::assertions_on_constants)]
    #[quickcheck]
    fn test_arbitrary_account(account: PublisherCapsAccount) {
        assert_eq!(account.header.magic_number, PC_MAGIC);
        assert_eq!(account.header.account_type, PC_ACCTYPE_SCORE);
        assert_eq!(account.header.version, PC_VERSION);
        assert_eq!(
            account.header.size as usize,
            size_of::<PublisherCapsAccount>()
        );
    }
}
