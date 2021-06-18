#include "pub_stats.hpp"
#include "misc.hpp"

using namespace pc;

pub_stats::pub_stats()
{
  clear_stats();
}

void pub_stats::clear_stats()
{
  num_agg_ = num_sent_ = num_recv_ = num_sub_drop_ =
    agg_slot_ = pub_slot_ = 0UL;
  __builtin_memset( shist_, 0, sizeof( shist_ ) );
}

uint64_t pub_stats::get_num_agg() const
{
  return num_agg_;
}

uint64_t pub_stats::get_num_sent() const
{
  return num_sent_;
}

uint64_t pub_stats::get_num_recv() const
{
  return num_recv_;
}

uint64_t pub_stats::get_num_sub_drop() const
{
  return num_sub_drop_;
}

double pub_stats::get_hit_rate() const
{
  return num_sent_ ? (100.*num_recv_)/num_sent_ : 0.;
}

void pub_stats::add_recv(
    uint64_t curr_slot, uint64_t agg_slot,  uint64_t pub_slot )
{
  if ( PC_UNLIKELY( num_sent_ == 0UL ) ) {
    return;
  }
  if ( pub_slot_ ) {
    uint64_t dslot = curr_slot>pub_slot?curr_slot - pub_slot:0UL;
    ++shist_[dslot<num_buckets?dslot:num_buckets-1];
  }
  if ( agg_slot != agg_slot_ ) {
    ++num_agg_;
    agg_slot_ = agg_slot;
  }
  if ( pub_slot != pub_slot_ ) {
    ++num_recv_;
    pub_slot_ = pub_slot;
  }
}

void pub_stats::get_slot_quartiles( uint32_t q[4] ) const
{
  uint64_t cum = 0;
  double pct[] = { 0.25, .50, .75, .99 };
  unsigned j=0;
  for( uint64_t i=0; i != num_buckets && j != 4; ++i ) {
    cum += shist_[i];
    while ( j != 4 && cum >= pct[j]*num_agg_ ) {
      q[j++] = i;
    }
  }
  for( ; j != 4; ++j ) {
    q[j] = num_buckets;
  }
}
