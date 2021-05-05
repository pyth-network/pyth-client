#include "pub_stats.hpp"
#include "misc.hpp"

using namespace pc;

pub_stats::pub_stats()
: num_sent_( 0 ),
  num_recv_( 0 )
{
  __builtin_memset( shist_, 0, sizeof( shist_ ) );
  __builtin_memset( thist_, 0, sizeof( shist_ ) );
}

uint64_t pub_stats::get_num_sent() const
{
  return num_sent_;
}

uint64_t pub_stats::get_num_recv() const
{
  return num_recv_;
}

uint64_t pub_stats::get_num_drop() const
{
  return num_sent_ - num_recv_;
}

double pub_stats::get_hit_rate() const
{
  return (100.*num_recv_)/num_sent_;
}

void pub_stats::add_send( uint64_t slot, int64_t ts )
{
  ++num_sent_;
  slots_.emplace_back( slot_time{ slot, ts } );
}

void pub_stats::add_recv(
    uint64_t curr_slot, uint64_t pub_slot, int64_t ts )
{
  static constexpr const int64_t  ts_bucket = 500000000L;
  for( ;!slots_.empty(); slots_.pop_front() ) {
    slot_time& it = slots_.front();
    if ( it.slot_ == pub_slot ) {
      ++num_recv_;
      uint64_t dt = std::max( 0L, (ts - it.ts_)/ts_bucket );
      uint64_t dslot = curr_slot>pub_slot?curr_slot - pub_slot:0UL;
      ++thist_[dt<num_buckets?dt:num_buckets-1];
      ++shist_[dslot<num_buckets?dslot:num_buckets-1];
      slots_.pop_front();
      break;
    } else if ( it.slot_ > pub_slot ) {
      break;
    }
  }
}

void pub_stats::get_quartiles( const uint32_t *hist, uint32_t q[4] ) const
{
  uint64_t cum = 0;
  double pct[] = { 0.25, .50, .75, .99 };
  unsigned j=0;
  for( uint64_t i=0; i != num_buckets && j != 4; ++i ) {
    cum += hist[i];
    while ( cum >= pct[j]*num_recv_ && j != 4) {
      q[j++] = i;
    }
  }
  for( ; j != 4; ++j ) {
    q[j] = num_buckets;
  }
}

void pub_stats::get_time_quartiles( float q[4] ) const
{
  uint32_t t[4];
  get_quartiles( thist_, t );
  for(unsigned i=0; i != 4; ++i ) {
    q[i] = t[i] * .5;
  }
}

void pub_stats::get_slot_quartiles( uint32_t q[4] ) const
{
  get_quartiles( shist_, q );
}
