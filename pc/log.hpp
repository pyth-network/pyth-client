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

  // log line
  class log_line
  {
  public:
    log_line& add( net_str key, net_str val );
    log_line& add( net_str key, int );
    log_line& add( net_str key, int64_t );
    log_line& add( net_str key, double );
    void end();
    friend class log;
  private:
    log_line( net_str, int lvl );
    void add_key( net_str );
    bool is_first_;
  };

  // log reporting
  // TODO: implement deferred (i.e. threaded) logging
  class log
  {
  public:

    static void set_level( int level );
    static bool has_level( int level );
    static log_line add( net_str topic, int level );
  private:
    static int level_;
  };

  inline bool log::has_level( int level )
  {
    return level&level_;
  }

}
