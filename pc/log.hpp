#pragma once

#include <pc/net_socket.hpp>

#define PC_LOG_DBG_LVL (1U<<3)
#define PC_LOG_INF_LVL (1U<<2)
#define PC_LOG_WRN_LVL (1U<<1)
#define PC_LOG_ERR_LVL (1U<<0)

#define PC_LOG_TXT(X,LVL) \
if (pc::log::has_level(LVL)) pc::log::add(X,LVL)
#define PC_LOG_DBG(X) PC_LOG_TXT(X,PC_LOG_DBG_LVL)
#define PC_LOG_INF(X) PC_LOG_TXT(X,PC_LOG_INF_LVL)
#define PC_LOG_WRN(X) PC_LOG_TXT(X,PC_LOG_WRN_LVL)
#define PC_LOG_ERR(X) PC_LOG_TXT(X,PC_LOG_ERR_LVL)

namespace pc
{

  class log_wtr : public net_wtr
  {
  public:
    void add_i64( int64_t );
    void add_u64( uint64_t );
    void add_f64( double );
  };

  // log line
  class log_line
  {
  public:
    log_line& add( str key, str val );
    log_line& add( str key, const pub_key& val );
    log_line& add( str key, const symbol& val );
    log_line& add( str key, int32_t );
    log_line& add( str key, int64_t );
    log_line& add( str key, uint64_t );
    log_line& add( str key, uint32_t );
    log_line& add( str key, double );
    void end();
    friend class log;
  private:
    log_line( str, int lvl );
    void add_key( str );
    bool    is_first_;
    log_wtr wtr_;
  };

  // log reporting
  class log
  {
  public:

    static void set_level( int level );
    static bool has_level( int level );
    static log_line add( str topic, int level );
  private:
    static int level_;
  };

  inline bool log::has_level( int level )
  {
    return level&level_;
  }

}
