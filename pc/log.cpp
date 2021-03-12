#include "log.hpp"
#include "misc.hpp"
#include <unistd.h>
#include <iostream>

using namespace pc;

int log::level_ = 0;

void log::set_level( int t )
{
  level_ = 1;
  while( level_ < (int)t ) {
    level_ <<= 1;
    level_ |= 1;
  }
}

log_line log::add( str topic, int level )
{
  return log_line( topic, level );
}

static const char spaces[] =
"                                                                         ";

static int log_pid = getpid();

log_line::log_line( str topic, int lvl )
: is_first_( true )
{
  char tbuf[32];
  nsecs_to_utc6( get_now(), tbuf );
  std::cerr << '[';
  std::cerr.write( tbuf, 27 );
  std::cerr << ' ' << log_pid << ' ';
  switch(lvl) {
    case PC_LOG_DBG_LVL: std::cerr << "DBG";break;
    case PC_LOG_INF_LVL: std::cerr << "INF";break;
    case PC_LOG_WRN_LVL: std::cerr << "WRN";break;
    case PC_LOG_ERR_LVL: std::cerr << "ERR";break;
  }
  std::cerr << ' ';
  const size_t topic_len = 40;
  size_t len = std::min( topic.len_, topic_len );
  std::cerr.write( topic.str_, len );
  if ( len < topic_len ) {
    std::cerr.write( spaces, topic_len-len );
  }
  std::cerr << "] ";
}

void log_line::add_key( str key )
{
  if ( !is_first_ ) {
    std::cerr << ',';
  } else {
    is_first_ = false;
  }
  std::cerr.write( key.str_, key.len_ );
  std::cerr << '=';
}

log_line& log_line::add( str key, str val )
{
  add_key( key );
  std::cerr.write( val.str_, val.len_ );
  return *this;
}

log_line& log_line::add( str key, int32_t val )
{
  add_key( key );
  std::cerr << val;
  return *this;
}

log_line& log_line::add( str key, int64_t val )
{
  add_key( key );
  std::cerr << val;
  return *this;
}

log_line& log_line::add( str key, uint64_t val )
{
  add_key( key );
  std::cerr << val;
  return *this;
}

log_line& log_line::add( str key, uint32_t val )
{
  add_key( key );
  std::cerr << val;
  return *this;
}

log_line& log_line::add( str key, double val )
{
  add_key( key );
  std::cerr << val;
  return *this;
}

log_line& log_line::add( str key, const pub_key& pk )
{
  add_key( key );
  std::string res;
  pk.enc_base58( res );
  std::cerr << res;
  return *this;
}

log_line& log_line::add( str key, const symbol& sym )
{
  add_key( key );
  std::cerr.write( sym.data(), symbol::len );
  return *this;
}

void log_line::end()
{
  std::cerr << std::endl;
}
