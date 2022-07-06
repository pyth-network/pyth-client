//using C style layout to make deserializing 
//particular entries easier to do
#[repr(C)]
/// Represents an SMA Tracker that has NUM_ENTRIES entries
/// each tracking GRANUALITY seconds.
struct SmaTracker <const GRANUALITY: u64, const NUM_ENTRIES: usize>{
    //timestamp of the last price update
    //prev_timestamp_
    valid_entry_counter: u64, // is incremented everytime we add a valid entry
    prev_entry_last_update: u64, //timestamp of the boundary with the previous entry    
    running_sums: [u64; NUM_ENTRIES], //SMA running suns
    current_entry: u8, //the current entry    
    entry_validity: [u8; NUM_ENTRIES],// the previous entry
    //TODO: Check if we need to multiples of 64
}