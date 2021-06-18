#pragma once

#include <stdint.h>

namespace pc
{

  // publish statistics
  class pub_stats
  {
  public:

    pub_stats();

    // number of prices submited
    uint64_t get_num_sent() const;

    // number of observed aggregate slot updates
    uint64_t get_num_agg() const;

    // number of observed publish slot updates
    uint64_t get_num_recv() const;

    // number of detected subscription service drop events
    // (i.e. where valid slot does not match publish slot of
    // previous update).
    uint64_t get_num_sub_drop() const;

    // hit rate is 100.*num_recv/num_sent or number of observed publisher
    // price updates relative to the number sent
    double get_hit_rate() const;

    // get (rough) quartiles of publish end-to-end latency in slots
    // up to a maximum of 32 slots
    void get_slot_quartiles( uint32_t q[4] ) const;

    // clear-down statistics
    void clear_stats();

    // add slot and the time it was received
    void add_recv( uint64_t slot, uint64_t agg_slot, uint64_t pub_slot );

    // increment subscription drop event
    void inc_sub_drop();

    // increment number of prices sent
    void inc_sent();

  private:

    static constexpr const uint64_t num_buckets = 32;

    uint64_t num_sent_;
    uint64_t num_recv_;
    uint64_t num_agg_;
    uint64_t num_sub_drop_;
    uint64_t agg_slot_;
    uint64_t pub_slot_;
    uint32_t shist_[num_buckets];
  };

  inline void pub_stats::inc_sub_drop()
  {
    ++num_sub_drop_;
  }

  inline void pub_stats::inc_sent()
  {
    ++num_sent_;
  }

}
