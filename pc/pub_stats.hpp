#pragma once

#include <stdint.h>
#include <deque>

namespace pc
{

  // publish statistics
  class pub_stats
  {
  public:

    pub_stats();

    // add slot and the time it was sent
    void add_send( uint64_t pub_slot, int64_t ts );

    // add slot and the time it was received
    void add_recv( uint64_t curr_slot, uint64_t pub_slot, int64_t ts );

    // get publish stats details
    uint64_t get_num_sent() const;
    uint64_t get_num_recv() const;
    uint64_t get_num_tx() const;
    uint64_t get_num_drop() const;

    // number of detected coalesce events (i.e. where valid slot does
    // not match publish slot of previous update). Does not indicate
    // how many submissions (transactions) were actually coalesced
    uint64_t get_num_coalesced() const;

    // hit rate is percentage of quotes sent that appeared in an aggregate
    // this may be undercounted because some submissions may be coalesced
    // into a single slot
    double get_hit_rate() const;

    // tx rate is the percentage of quotes send that are confirmed
    double get_tx_rate() const;

    // get (rough) quartiles of publish end-to-end latency in seconds
    // to a max of 16 seconds
    void get_time_quartiles( float q[4] ) const;

    // get (rough) quartiles of publish end-to-end latency in slots
    // up to a maximum of 32 slots
    void get_slot_quartiles( uint32_t q[4] ) const;

    // clear-down statistics
    void clear_stats();

    // increment signature count
    void inc_tx();

    // increment coalesce event count
    void inc_coalesce();

  private:

    struct slot_time {
      uint64_t slot_;
      int64_t  ts_;
    };

    typedef std::deque<slot_time> slots_t;

    static constexpr const uint64_t num_buckets = 32;

    void get_quartiles( const uint32_t *hist, uint32_t q[4] ) const;

    slots_t  slots_;
    uint64_t num_sent_;
    uint64_t num_tx_;
    uint64_t num_recv_;
    uint64_t num_coal_;
    uint32_t thist_[num_buckets];
    uint32_t shist_[num_buckets];
  };

  inline void pub_stats::inc_tx()
  {
    ++num_tx_;
  }

  inline void pub_stats::inc_coalesce()
  {
    ++num_coal_;
  }

}
