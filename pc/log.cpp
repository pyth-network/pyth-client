#include "log.hpp"
#include "misc.hpp"
#include <unistd.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <iostream>

namespace pc
{

  class log_impl
  {
  public:
    log_impl();
    ~log_impl();
    void start();
    void stop();
    void run();
    void add( net_wtr& wtr );

  private:
    typedef std::vector<net_buf*> buf_vec_t;
    typedef std::atomic<bool> atomic_t;
    atomic_t    is_run_;
    atomic_t    is_wtr_;
    std::mutex  mtx_;
    std::thread thrd_;
    buf_vec_t   logv_;
    buf_vec_t   reuse_;
  };

}

using namespace pc;

static void run_log( log_impl *iptr )
{
  iptr->run();
}

log_impl::log_impl()
: is_run_( true ),
  is_wtr_( false )
{
}

log_impl::~log_impl()
{
  stop();
}

void log_impl::start()
{
  if ( !thrd_.joinable() ) {
    thrd_ = std::thread( run_log, this );
  }
}

void log_impl::stop()
{
  is_run_ = false;
  if ( thrd_.joinable() ) {
    thrd_.join();
  }
  for( net_buf *ptr: reuse_ ) {
    while( ptr ) {
      net_buf *nxt = ptr->next_;
      delete ptr;
      ptr = nxt;
    }
  }
  reuse_.clear();
}

void log_impl::add( net_wtr& wtr )
{
  buf_vec_t reuse;
  net_buf *hd, *tl;
  wtr.detach( hd, tl );
  mtx_.lock();
  logv_.push_back( hd );
  reuse.swap( reuse_ );
  is_wtr_ = true;
  mtx_.unlock();
  for( net_buf *ptr: reuse ) {
    while( ptr ) {
      net_buf *nxt = ptr->next_;
      ptr->dealloc();
      ptr = nxt;
    }
  }
}

void log_impl::run()
{
  struct timespec ts[1];
  ts->tv_sec  = 0;
  ts->tv_nsec = 1000000;
  buf_vec_t logv;
  for(;;) {
    if ( is_wtr_ ) {
      // get buffers to log
      mtx_.lock();
      is_wtr_ = false;
      logv_.swap( logv );
      mtx_.unlock();

      // write the buffers to stderr
      for( net_buf *ptr: logv ) {
        while( ptr ) {
          net_buf *nxt = ptr->next_;
          std::cerr.write( ptr->buf_, ptr->size_ );
          ptr = nxt;
        }
        std::cerr << std::endl;
      }

      // send buffers back
      mtx_.lock();
      std::copy( logv.begin(), logv.end(), std::back_inserter( reuse_ ) );
      mtx_.unlock();
      logv.clear();
    } else if ( !is_run_ ) {
      break;
    } else {
      // sleep a bit
      clock_nanosleep( CLOCK_REALTIME, 0, ts, NULL );
    }
  }
}

int log::level_ = 0;
static log_impl impl_;

void log::set_level( int t )
{
  level_ = 1;
  while( level_ < (int)t ) {
    level_ <<= 1;
    level_ |= 1;
  }
  impl_.start();
}

log_line log::add( str topic, int level )
{
  return log_line( topic, level );
}

static const char spaces[] =
"                                                                         ";

static int log_pid = getpid();

void log_wtr::add_i64( int64_t val )
{
  char *buf = reserve( 32 );
  sprintf( buf, "%ld", val );
  advance( __builtin_strlen( buf ) );
}

void log_wtr::add_u64( uint64_t val )
{
  char *buf = reserve( 32 );
  sprintf( buf, "%lu", val );
  advance( __builtin_strlen( buf ) );
}

void log_wtr::add_f64( double val )
{
  char *buf = reserve( 32 );
  sprintf( buf, "%f", val );
  advance( __builtin_strlen( buf ) );
}

log_line::log_line( str topic, int lvl )
: is_first_( true )
{
  char tbuf[32];
  nsecs_to_utc6( get_now(), tbuf );
  wtr_.add( '[' );
  wtr_.add( str( tbuf, 27 ) );
  wtr_.add( ' ' );
  wtr_.add_i64( log_pid );
  wtr_.add( ' ' );
  switch(lvl) {
    case PC_LOG_DBG_LVL: wtr_.add( "DBG" );break;
    case PC_LOG_INF_LVL: wtr_.add( "INF" );break;
    case PC_LOG_WRN_LVL: wtr_.add( "WRN" );break;
    case PC_LOG_ERR_LVL: wtr_.add( "ERR" );break;
  }
  wtr_.add( ' ' );
  const size_t topic_len = 40;
  size_t len = std::min( topic.len_, topic_len );
  wtr_.add( str( topic.str_, len ) );
  if ( len < topic_len ) {
    wtr_.add( str( spaces, topic_len-len ) );
  }
  wtr_.add( ']' );
  wtr_.add( ' ' );
}

void log_line::add_key( str key )
{
  if ( !is_first_ ) {
    wtr_.add( ',' );
  } else {
    is_first_ = false;
  }
  wtr_.add( key );
  wtr_.add( '=' );
}

log_line& log_line::add( str key, str val )
{
  add_key( key );
  wtr_.add( val );
  return *this;
}

log_line& log_line::add( str key, int32_t val )
{
  add_key( key );
  wtr_.add_i64( val );
  return *this;
}

log_line& log_line::add( str key, int64_t val )
{
  add_key( key );
  wtr_.add_i64( val );
  return *this;
}

log_line& log_line::add( str key, uint64_t val )
{
  add_key( key );
  wtr_.add_u64( val );
  return *this;
}

log_line& log_line::add( str key, uint32_t val )
{
  add_key( key );
  wtr_.add_u64( val );
  return *this;
}

log_line& log_line::add( str key, double val )
{
  add_key( key );
  wtr_.add_f64( val );
  return *this;
}

log_line& log_line::add( str key, const pub_key& pk )
{
  add_key( key );
  std::string res;
  pk.enc_base58( res );
  wtr_.add( res );
  return *this;
}

log_line& log_line::add( str key, const symbol& sym )
{
  add_key( key );
  wtr_.add( sym.as_str() );
  return *this;
}

void log_line::end()
{
  impl_.add( wtr_ );
  wtr_.reset();
}
