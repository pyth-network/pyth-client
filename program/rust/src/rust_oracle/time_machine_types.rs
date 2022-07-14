//TODO: Move validated Circular Buffer into it's own class
//TODO: Handle overflows
//TODO: Find a way to use macros to make the wrapper
use std::cmp;
fn get_requested_entry_offset(granuality : u64, last_update_time:u64, entry_time:u64)->usize{
    let entry_time = entry_time % granuality;
    if last_update_time < entry_time{
        panic!("prices unavailable yet");
    }
    ((last_update_time - entry_time + granuality - 1) / granuality).try_into().unwrap()
}

//using C style layout to make deserializing
//particular entries easier to do
#[derive(Debug, Clone)]
#[repr(C)]
/// Represents an SMA Tracker that has NUM_ENTRIES entries
/// each tracking GRANUALITY seconds.
///The prices are assumed to be provided under some fixed point representation, and the computation
/// gurantees accuracy up to the last decimal digit in the fixed point representation.
struct SmaTracker<const GRANUALITY: u64, const NUM_ENTRIES: usize, const THRESHOLD: u64> {
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

    fn first_add_price(&mut self, current_time: u64, prev_time: u64, new_price: u64, old_price: u64){
        //make sure the type we missed in the current entry is reflected is max_update_time to get
        //correct validities at the end
        let missed_time = prev_time % GRANUALITY;
        self.max_update_time = cmp::max(self.max_update_time, missed_time);
        self.add_price(current_time, prev_time, new_price, old_price);
        

    }

    fn get_entry(&self, last_update_time: u64, entry_time: u64) -> (u64, u8) {
        //FIXME: handle overflow, left for until we settle on integer type used
        let offset = get_requested_entry_offset(GRANUALITY, last_update_time, entry_time) % NUM_ENTRIES;
        
        if offset > NUM_ENTRIES - 2 {
            return (0, 0);
        }
        let entry = (self.current_entry - offset + NUM_ENTRIES) % NUM_ENTRIES;
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
struct TickTracker<const GRANUALITY: u64, const NUM_ENTRIES: usize, const THRESHOLD: u64> {
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
    fn first_add_price(&mut self, current_time: u64, prev_time: u64, new_price: u64, old_price: u64){
        //this is just here for consistency. optimizerr should inline this
        self.add_price(current_time, prev_time, new_price, old_price);
    }
    fn get_entry(&self, last_update_time: u64, entry_time: u64) -> (u64, u8) {
        let offset = get_requested_entry_offset(GRANUALITY, last_update_time, entry_time) % NUM_ENTRIES;
        if offset > NUM_ENTRIES - 1 {
            return (0, 0);
        }
        let entry = (self.current_entry - offset + NUM_ENTRIES) % NUM_ENTRIES;
        (self.ticks[entry], self.entry_validity[entry])
    }
}

const SOLANA_SLOT_TIME: u64 = 400;
const THIRTY_MINUTES: u64 = 30 * 60 * 1000; 
const FIVE_MINUTES: u64 = 5 * 60 * 1000;
const THIRTY_SECONDS: u64 = 30 * 1000;
//TODO: Look into making a macro that generates the class definition below as well as the 
//methods.
#[derive(Debug, Clone)]
#[repr(C)]
/// this wraps all the structs we plan on using
pub struct TimeMachineWrapper{
    //TODO: Maybe Threshold and Granuality Should be related for SMAs!
    //30 Minute TWAPs for the last day
    sma_tracker1: SmaTracker<THIRTY_MINUTES, 50, {20 * SOLANA_SLOT_TIME}>,
    //5 minute TWAPs for the last 2 hours
    sma_tracker2: SmaTracker<FIVE_MINUTES, 26, {5 * SOLANA_SLOT_TIME}>,
    //30 seconds TWAPs for the last 10 hours
    sma_tracker3: SmaTracker<THIRTY_SECONDS, 22, {5 * SOLANA_SLOT_TIME}>,
    //30 Minute ticks for the last day
    tick_tracker1: TickTracker<THIRTY_MINUTES, 49, {20 * SOLANA_SLOT_TIME}>,
    //5 minute ticks for the last 2 hours
    tick_tracker2: TickTracker<FIVE_MINUTES, 25, {5 * SOLANA_SLOT_TIME}>,
    //30 seconds TWAPs for the last 10 hours
    tick_tracker3: TickTracker<THIRTY_SECONDS, 21, {5 * SOLANA_SLOT_TIME}>,
}



impl TimeMachineWrapper{
    fn add_price(&mut self, current_time: u64, prev_time: u64, new_price: u64, old_price: u64) {
        self.sma_tracker1.add_price(current_time, prev_time, new_price, old_price);
        self.sma_tracker2.add_price(current_time, prev_time, new_price, old_price);
        self.sma_tracker3.add_price(current_time, prev_time, new_price, old_price);
        self.tick_tracker1.add_price(current_time, prev_time, new_price, old_price);
        self.tick_tracker2.add_price(current_time, prev_time, new_price, old_price);
        self.tick_tracker3.add_price(current_time, prev_time, new_price, old_price);
    }
    fn first_add_price(&mut self, current_time: u64, prev_time: u64, new_price: u64, old_price: u64) {
        self.sma_tracker1.first_add_price(current_time, prev_time, new_price, old_price);
        self.sma_tracker2.first_add_price(current_time, prev_time, new_price, old_price);
        self.sma_tracker3.first_add_price(current_time, prev_time, new_price, old_price);
        self.tick_tracker1.first_add_price(current_time, prev_time, new_price, old_price);
        self.tick_tracker2.first_add_price(current_time, prev_time, new_price, old_price);
        self.tick_tracker3.first_add_price(current_time, prev_time, new_price, old_price);
    }
    /// gets the given entry from the tracker with the
    fn get_entry(&self, current_time: u64, entry_time: u64,granuality: u64, is_twap: bool) -> (u64, u8) {
        match (granuality, is_twap){
            (THIRTY_MINUTES, true) => self.sma_tracker1.get_entry(current_time, entry_time),
            (FIVE_MINUTES, true) => self.sma_tracker2.get_entry(current_time, entry_time),
            (THIRTY_SECONDS, true) => self.sma_tracker3.get_entry(current_time, entry_time),
            (THIRTY_MINUTES, false) => self.tick_tracker1.get_entry(current_time, entry_time),
            (FIVE_MINUTES, false) => self.tick_tracker2.get_entry(current_time, entry_time),
            (THIRTY_SECONDS, false) => self.tick_tracker3.get_entry(current_time, entry_time),
            _ => panic!("incorrect tracker"),
        }
    }
}