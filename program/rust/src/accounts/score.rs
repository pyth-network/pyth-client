use {
    super::{
        AccountHeader,
        PythAccount,
    },
    crate::c_oracle_header::{
        PC_ACCTYPE_SCORE,
        PC_MAX_PUBLISHERS,
        PC_MAX_SYMBOLS,
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
#[derive(Copy, Clone)]
pub struct PublisherScoresAccount {
    pub header:                AccountHeader,
    pub num_publishers:        usize,
    pub num_symbols:           usize,
    // a constant used to normalize the scores
    pub z:                     u32,
    pub publisher_permissions: [[bool; PC_MAX_SYMBOLS as usize]; PC_MAX_PUBLISHERS as usize],
    pub scores:                [f64; PC_MAX_PUBLISHERS as usize],
    pub publishers:            [Pubkey; PC_MAX_PUBLISHERS as usize],
    pub symbols:               [Pubkey; PC_MAX_SYMBOLS as usize],
}

impl PublisherScoresAccount {
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
            .symbols
            .iter()
            .position(|&x| x == price_account)
            .ok_or(ProgramError::InvalidArgument)?;

        self.publisher_permissions[publisher_index][symbol_index] = true;
        self.calculate_scores()
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

        let symbol_index = self
            .symbols
            .iter()
            .position(|&x| x == price_account)
            .ok_or(ProgramError::InvalidArgument)?;

        self.publisher_permissions[publisher_index][symbol_index] = false;
        self.calculate_scores()
    }

    pub fn add_price(&mut self, symbol: Pubkey) -> Result<(), ProgramError> {
        let symbol_index = self
            .symbols
            .iter()
            .position(|&x| x == symbol)
            .unwrap_or(self.num_symbols);

        if symbol_index == self.num_symbols {
            if self.num_symbols == PC_MAX_SYMBOLS as usize {
                return Err(ProgramError::AccountDataTooSmall);
            }

            self.symbols[self.num_symbols] = symbol;
            self.num_symbols += 1;
        }

        Ok(())
    }

    pub fn del_price(&mut self, symbol: Pubkey) -> Result<(), ProgramError> {
        let symbol_index = self
            .symbols
            .iter()
            .position(|&x| x == symbol)
            .ok_or(ProgramError::InvalidArgument)?;

        self.symbols[symbol_index] = self.symbols[self.num_symbols - 1];
        self.symbols[self.num_symbols - 1] = Pubkey::default();
        self.num_symbols -= 1;
        self.calculate_scores()
    }

    pub fn calculate_scores(&mut self) -> Result<(), ProgramError> {
        let symbol_scores: Vec<u32> = self
            .symbols
            .iter()
            .enumerate()
            .map(|(j, _)| {
                let score = self
                    .publisher_permissions
                    .iter()
                    .fold(0, |score, permissions| score + permissions[j] as u32);
                max(score, self.z)
            })
            .collect();

        for i in 0..self.num_publishers {
            self.scores[i] = self
                .symbols
                .iter()
                .enumerate()
                .filter(|(j, _)| self.publisher_permissions[i][*j])
                .map(|(j, _)| 1f64 / symbol_scores[j] as f64)
                .sum();
        }
        Ok(())
    }
}

impl PythAccount for PublisherScoresAccount {
    const ACCOUNT_TYPE: u32 = PC_ACCTYPE_SCORE;
    // Calculate the initial size of the account
    const INITIAL_SIZE: u32 = 1000;
}

// Unsafe impl because product_list is of size 640 and there's no derived trait for this size
unsafe impl Pod for PublisherScoresAccount {
}

unsafe impl Zeroable for PublisherScoresAccount {
}
