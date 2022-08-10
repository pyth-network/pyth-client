use std::collections::HashSet;
use std::mem::size_of;

use crate::c_oracle_header::{
    cmd_del_publisher,
    cmd_hdr_t,
    command_t_e_cmd_add_product,
    command_t_e_cmd_del_publisher,
    pc_map_table_t,
    pc_price_t,
    pc_prod_t,
    pc_pub_key_t,
    PC_ACCTYPE_PRODUCT,
    PC_COMP_SIZE,
    PC_MAGIC,
    PC_MAP_TABLE_SIZE,
    PC_PROD_ACC_SIZE,
    PC_VERSION,
};
use crate::deserialize::{
    load_account_as,
    load_mut,
};
use crate::rust_oracle::{
    add_product,
    add_publisher,
    clear_account,
    del_publisher,
    initialize_checked,
    load_checked,
    pubkey_equal,
};
use crate::tests::test_utils::AccountSetup;
use bytemuck::bytes_of;
use quickcheck::{
    Arbitrary,
    Gen,
};
use quickcheck_macros::quickcheck;
use rand::Rng;
use solana_program::account_info::AccountInfo;
use solana_program::clock::Epoch;
use solana_program::program_error::ProgramError;
use solana_program::pubkey::Pubkey;
use solana_program::rent::Rent;
use std::fmt::{
    Debug,
    Formatter,
};


#[derive(Clone, Debug)]
enum Operation {
    Add(pc_pub_key_t),
    Delete,
}

impl Debug for pc_pub_key_t {
    fn fmt(&self, f: &mut Formatter<'_>) -> Result<(), std::fmt::Error> {
        unsafe { return self.k8_.fmt(f) }
    }
}

impl pc_price_t {
    fn to_set(&self) -> HashSet<[u64; 4]> {
        let mut res: HashSet<[u64; 4]> = HashSet::new();
        for i in 0..(self.num_ as usize) {
            unsafe { res.insert(self.comp_[i].pub_.k8_) };
        }
        return res;
    }
}

impl Arbitrary for pc_pub_key_t {
    fn arbitrary(g: &mut Gen) -> Self {
        let mut array = [0u64; 4];
        for i in 0..3 {
            array[i] = u64::arbitrary(g);
        }
        return pc_pub_key_t { k8_: array };
    }
}

impl Arbitrary for Operation {
    fn arbitrary(g: &mut Gen) -> Self {
        let sample = u8::arbitrary(g);
        match sample % 3 {
            0 => {
                return Operation::Add(pc_pub_key_t::arbitrary(g));
            }
            1 => {
                return Operation::Add(pc_pub_key_t::arbitrary(g));
            }
            2 => {
                return Operation::Delete;
            }
            _ => panic!(),
        }
    }
}

fn populate_data(instruction_data: &mut [u8], publisher: pc_pub_key_t) {
    let mut hdr = load_mut::<cmd_del_publisher>(instruction_data).unwrap();
    hdr.ver_ = PC_VERSION;
    hdr.cmd_ = command_t_e_cmd_del_publisher as i32;
    hdr.pub_ = publisher;
}

#[quickcheck]
fn prop(input: Vec<Operation>) -> bool {
    let mut set: HashSet<[u64; 4]> = HashSet::new();

    let program_id = Pubkey::new_unique();

    let mut funding_setup = AccountSetup::new_funding();
    let funding_account = funding_setup.to_account_info();

    let mut instruction_data = [0u8; size_of::<cmd_del_publisher>()];

    let mut price_setup = AccountSetup::new::<pc_price_t>(&program_id);
    let price_account = price_setup.to_account_info();
    initialize_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();

    let mut rng = rand::thread_rng();

    for op in input {
        match op {
            Operation::Add(pub_) => {
                populate_data(&mut instruction_data, pub_);

                if let Err(err) = add_publisher(
                    &program_id,
                    &[funding_account.clone(), price_account.clone()],
                    &instruction_data,
                ) {
                    {
                        assert_eq!(err, ProgramError::InvalidArgument);
                        let price_data =
                            load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
                        if price_data.num_ != PC_COMP_SIZE {
                            return false;
                        }
                    }
                } else {
                    unsafe { set.insert(pub_.k8_) };
                }
            }
            Operation::Delete => {
                let pubkey_to_delete: pc_pub_key_t;
                {
                    let price_data =
                        load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
                    if price_data.num_ != 0 {
                        let i: usize = rng.gen_range(0..(price_data.num_ as usize));
                        pubkey_to_delete = price_data.comp_[i].pub_;
                    } else {
                        pubkey_to_delete = pc_pub_key_t::new_unique();
                    }
                    populate_data(&mut instruction_data, pubkey_to_delete);
                }

                if let Err(err) = del_publisher(
                    &program_id,
                    &[funding_account.clone(), price_account.clone()],
                    &instruction_data,
                ) {
                    {
                        assert_eq!(err, ProgramError::InvalidArgument);
                        let price_data =
                            load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
                        if price_data.num_ != 0 {
                            return false;
                        }
                    }
                } else {
                    unsafe { set.remove(&pubkey_to_delete.k8_) };
                }
            }
        }
    }
    {
        let price_data = load_checked::<pc_price_t>(&price_account, PC_VERSION).unwrap();
        if price_data.to_set() != set {
            return false;
        }
    }
    return true;
}
