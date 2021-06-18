#include <pc/replay.hpp>
#include <pc/mem_map.hpp>
#include <pc/misc.hpp>
#include <algorithm>
#include <iostream>
#include <vector>
#include <deque>
#include <map>

using namespace pc;

class slot_replay : public error
{
public:
  slot_replay();

  // test_slot capture file
  void set_file( const std::string& file );
  std::string get_file() const;

  // initialize
  bool init();

  // get next slot update
  bool get_next();

  // get slot number
  uint64_t get_slot() const;
  void inc_miss();

  void report();

private:

  struct stats
  {
    stats();
    uint64_t miss_;
    uint64_t tot_;
  };

  typedef std::map<std::string,stats> stats_t;

  uint64_t    slot_;
  str         ldr_;
  stats      *sptr_;
  const char *ptr_;
  const char *end_;
  mem_map     mbuf_;
  std::string file_;
  stats_t     stats_;
};

class pub_replay : public error
{
public:
  pub_replay();

  // pyth capture file
  void set_file( const std::string& file );
  std::string get_file() const;

  // initialize
  bool init();

  // get next slot update
  bool get_next();
  uint64_t get_slot() const;

private:
  typedef std::deque<uint64_t> slots_t;

  replay   rep_;
  uint64_t slot_;
  size_t   num_;
  slots_t  slots_;
};

slot_replay::slot_replay()
: slot_( 0UL )
{
}

void slot_replay::set_file( const std::string& file )
{
  file_ = file;
}

std::string slot_replay::get_file() const
{
  return file_;
}

bool slot_replay::init()
{
  mbuf_.set_file( file_ );
  if ( !mbuf_.init() ) {
    return set_err_msg( "failed to read " + file_ );
  }
  ptr_ = mbuf_.data();
  end_ = &ptr_[mbuf_.size()];
  // skip headers
  for(;;++ptr_) {
    if ( ptr_ == end_ ) return false;
    if ( *ptr_ == '\n' ) break;
  }
  ++ptr_;
  return get_next();
}

slot_replay::stats::stats()
: miss_( 0UL ),
  tot_( 0UL )
{
}

bool slot_replay::get_next()
{
  const char *ptr = ptr_;
  // timestamp
  for(;;++ptr) {
    if ( ptr == end_ ) return false;
    if ( *ptr == ',' ) break;
  }
  // slot
  const char *sptr = ++ptr;
  for(;;++ptr) {
    if ( ptr == end_ ) return false;
    if ( *ptr == ',' ) break;
  }
  slot_ = str_to_uint( sptr, ptr-sptr );
  // key
  ldr_.str_ = ++ptr;
  for(;;++ptr) {
    if ( ptr == end_ ) return false;
    if ( *ptr == '\n' ) break;
  }
  ldr_.len_ = ptr - ldr_.str_;
  ptr_ = ++ptr;

  // update stats
  std::string lstr( ldr_.as_string() );
  sptr_ = &stats_[lstr];
  ++sptr_->tot_;

  return true;
}

void slot_replay::report()
{
  std::cout << "leader,num_miss,num_sent" << std::endl;
  for( stats_t::iterator i = stats_.begin(); i != stats_.end(); ++i ) {
    stats& st = (*i).second;
    std::cout << (*i).first << ','
              << st.miss_ << ','
              << st.tot_
              << std::endl;
  }
}

inline uint64_t slot_replay::get_slot() const
{
  return slot_;
}

inline void slot_replay::inc_miss()
{
  ++sptr_->miss_;
}

pub_replay::pub_replay()
: slot_( 0 ),
  num_( 0UL )
{
}

void pub_replay::set_file( const std::string& file )
{
  rep_.set_file( file );
}

std::string pub_replay::get_file() const
{
  return rep_.get_file();
}

bool pub_replay::init()
{
  num_ = 1UL;
  slots_.clear();
  if ( !rep_.init() ) {
    return set_err_msg( rep_.get_err_msg() );
  }
  return get_next();
}

bool pub_replay::get_next()
{
  while( slots_.size() < num_ ) {
    if ( rep_.get_next() ) {
      pc_price_t *aptr = (pc_price_t*)rep_.get_update();
      if ( aptr->type_ == PC_ACCTYPE_PRICE ) {
        slots_.push_back( aptr->agg_.pub_slot_ );
        for( unsigned i=slots_.size()-1; i!=0; --i ) {
          if ( slots_[i] < slots_[i-1] ) {
            std::swap( slots_[i], slots_[i-1] );
          } else {
            break;
          }
        }
      } else if ( aptr->type_ == PC_ACCTYPE_PRODUCT ) {
        ++num_;
      }
    } else {
      return false;
    }
  }
  slot_ = slots_.front();
  slots_.pop_front();
  return true;
}

inline uint64_t pub_replay::get_slot() const
{
  return slot_;
}

int usage()
{
  std::cerr << "usage: leader_stats <cap_file> <slots_csv>"
            << std::endl;
  return 1;
}

int main( int argc, char **argv)
{
  // get input params and options
  if ( argc < 3 ) {
    return usage();
  }
  std::string cap_file = argv[1];
  std::string slt_file = argv[2];

  // initialize capture replay
  pub_replay rep;
  rep.set_file( cap_file );
  if ( !rep.init() ) {
    std::cerr << "leader_stats: " << rep.get_err_msg() << std::endl;
    return 1;
  }

  // initialize slot replay
  slot_replay slt;
  slt.set_file( slt_file );
  if ( !slt.init() ) {
    std::cerr << "leader_stats: " << slt.get_err_msg() << std::endl;
    return 1;
  }

  // catch up to first pub slot
  while( slt.get_slot() < rep.get_slot() ) {
    if ( !slt.get_next() ) {
      return 0;
    }
  }

  // calculate leader misses
  for(;;) {
    uint64_t tslot = slt.get_slot();
    uint64_t pslot = rep.get_slot();
    if ( tslot == pslot ) {
      if ( !slt.get_next() || !rep.get_next() ) {
        break;
      }
    } else if ( tslot < pslot ) {
      // missing slot
      slt.inc_miss();
      if ( !slt.get_next() ) {
        break;
      }
    } else if ( tslot > pslot ) {
      // ahead of pub slot
      if ( !rep.get_next() ) {
        break;
      }
    }
  }

  // report misses
  slt.report();

  return 0;
}
