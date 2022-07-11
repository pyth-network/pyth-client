use std::cmp;

//using C style layout to make deserializing
//particular entries easier to do
#[derive(Debug, Clone)]
#[repr(C)]
/// Represents an SMA Tracker that has NUM_ENTRIES entries
/// each tracking GRANUALITY seconds.
///The prices are assumed to be provided under some fixed point representation, and the computation
/// gurantees accuracy up to the last decimal digit in the fixed point representation.
pub struct SmaTracker<const GRANUALITY: u64, const NUM_ENTRIES: usize, const THRESHOLD: u64> {
    valid_entry_counter: u64, // is incremented everytime we add a valid entry
    max_update_time: u64,     // maximum time between two updates in the current entry.
    running_sums: [u64; NUM_ENTRIES], //SMA running suns
    current_entry: usize,     //the current entry
    entry_validity: [u8; NUM_ENTRIES], // the previous entry
                              //TODO: Check if we need to multiples of 64
}

impl<const GRANUALITY: u64, const NUM_ENTRIES: usize, const THRESHOLD: u64>
    SmaTracker<GRANUALITY, NUM_ENTRIES, THRESHOLD>
{
    fn add_price(&mut self, current_time: u64, prev_time: u64, new_price: u64, old_price: u64) {
        let inflated_new_price = new_price * 10;
        let inflated_old_price = old_price * 10;
        let update_time = current_time - prev_time;
        self.max_update_time = cmp::max(self.max_update_time, update_time);
        if prev_time % GRANUALITY + update_time < GRANUALITY {
            //WARRNING: DIVISION
            self.running_sums[self.current_entry] +=
                update_time * (inflated_old_price + inflated_new_price) / 2;
        } else {
            //update current price
            let new_price_weight = current_time % GRANUALITY;
            let old_price_weight = update_time - new_price_weight;
            let interpolated_border_price = (inflated_old_price * new_price_weight
                + inflated_new_price * old_price_weight)
                / update_time;
            self.running_sums[self.current_entry] +=
                old_price_weight * (inflated_old_price + interpolated_border_price) / 2;

            if self.max_update_time <= THRESHOLD {
                self.entry_validity[self.current_entry] = 1;
                self.valid_entry_counter += 1;
            }
            //update next price
            let next_entry = (self.current_entry + 1) % NUM_ENTRIES;
            self.entry_validity[next_entry] = 0;
            self.running_sums[next_entry] = self.running_sums[self.current_entry]
                + new_price_weight * (inflated_new_price + interpolated_border_price) / 2;
            //update current_entry state
            self.current_entry = next_entry;
            self.max_update_time = update_time;
        }
    }
    fn get_entry(&self, entry: usize) -> (u64, u8) {
        //FIXME: handle overflow, left for until we settle on integer type used
        if (self.current_entry - entry + NUM_ENTRIES) % NUM_ENTRIES > NUM_ENTRIES - 2 {
            return (0, 0);
        }

        (
            ((self.running_sums[entry]
                - self.running_sums[(entry + NUM_ENTRIES - 1) % NUM_ENTRIES])
                / GRANUALITY
                + 5)
                / 10,
            self.entry_validity[entry],
        )
    }
}

#[derive(Debug, Clone)]
#[repr(C)]
/// Represents an Tick Tracker that has NUM_ENTRIES entries
/// each storing the last price before multiples of GRANUALITY seconds.
pub struct TickTracker<const GRANUALITY: u64, const NUM_ENTRIES: usize, const THRESHOLD: u64> {
    ticks: [u64; NUM_ENTRIES], //SMA running suns
    current_entry: usize,      //the current entry
    entry_validity: [u8; NUM_ENTRIES], // the previous entry
                               //TODO: Check if we need to multiples of 64
}

impl<const GRANUALITY: u64, const NUM_ENTRIES: usize, const THRESHOLD: u64>
    TickTracker<GRANUALITY, NUM_ENTRIES, THRESHOLD>
{
    fn add_price(&mut self, current_time: u64, prev_time: u64, new_price: u64, old_price: u64) {
        let update_time = current_time - prev_time;
        if prev_time % GRANUALITY + update_time < GRANUALITY {
            //WARRNING: DIVISION
            self.ticks[self.current_entry] = new_price;
        } else {
            if GRANUALITY - (prev_time % GRANUALITY) <= THRESHOLD {
                self.entry_validity[self.current_entry] = 1;
            }
            self.current_entry = (self.current_entry + 1) % NUM_ENTRIES;
            self.ticks[self.current_entry] = new_price;
        }
    }
    fn get_entry(&self, entry: usize) -> (u64, u8) {
        (self.ticks[entry], self.entry_validity[entry])
    }
}
