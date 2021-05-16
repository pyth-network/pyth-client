#include "tx_svr.hpp"
#include <pc/log.hpp>
#include <pc/misc.hpp>
#include <unistd.h>
#include <signal.h>
#include <iostream>

using namespace pc;

// pyth tx_svr server for forwarding transactions to
// current and next leader in schedule

std::string get_rpc_host()
{
  return "localhost";
}

int get_port()
{
  return 8898;
}

int usage()
{
  std::cerr << "usage: pyth_tx_svr <options>" << std::endl;
  std::cerr << "options include:" << std::endl;
  std::cerr << "  -r <rpc_host (default " << get_rpc_host() << ")>"
            << std::endl;
  std::cerr << "     Host name or IP address of solana rpc node in the form "
               "host_name[:rpc_port[:ws_port]]\n" << std::endl;
  std::cerr << "  -p <listening_port (default " << get_port() << ">"
            << std::endl;
  std::cerr << "     tcp/ip port for pyth-client clients to connect to\n"
            << std::endl;
  std::cerr << "  -l <log_file>" << std::endl;
  std::cerr << "     Optional log file - uses stderr if not provided\n"
            << std::endl;
  std::cerr << "  -n" << std::endl;
  std::cerr << "     No wait mode - i.e. run using busy poll loop\n"
            << std::endl;
  std::cerr << "  -d" << std::endl;
  std::cerr << "     Turn on debug logging. Can also toggle this on/off via "
               "kill -s SIGUSR1 <pid>\n" << std::endl;
  return 1;
}

bool do_run = true;

void sig_handle( int )
{
  do_run = false;
}

void sig_toggle( int )
{
  // toggle between debug and info logging
  if ( log::has_level( PC_LOG_DBG_LVL ) ) {
    log::set_level( PC_LOG_INF_LVL );
  } else {
    log::set_level( PC_LOG_DBG_LVL );
  }
}

int main(int argc, char **argv)
{
  // command-line parsing
  std::string log_file;
  std::string rpc_host = get_rpc_host();
  int opt = 0, pyth_port = get_port();
  bool do_wait = true, do_debug = false;
  while( (opt = ::getopt(argc,argv, "r:p:l:dnh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'p': pyth_port = ::atoi(optarg); break;
      case 'd': do_debug = true; break;
      case 'l': log_file = optarg; break;
      case 'n': do_wait = false; break;
      default: return usage();
    }
  }

  // set up logging and disable SIGPIPE
  signal( SIGPIPE, SIG_IGN );
  if ( !log_file.empty() && !log::set_log_file( log_file ) ) {
    std::cerr << "pyth_tx: failed to create log_file="
              << log_file << std::endl;
    return 1;
  }
  log::set_level( do_debug ? PC_LOG_DBG_LVL : PC_LOG_INF_LVL );

  // construct and initialize tx_svr server
  tx_svr mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_listen_port( pyth_port );
  if ( !mgr.init() ) {
    std::cerr << "pyth_tx: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // set up signal handing
  signal( SIGINT, sig_handle );
  signal( SIGHUP, sig_handle );
  signal( SIGTERM, sig_handle );
  signal( SIGUSR1, sig_toggle );
  while( do_run && !mgr.get_is_err() ) {
    mgr.poll( do_wait );
  }
  int retcode = 0;
  if ( mgr.get_is_err() ) {
    std::cerr << "pyth_tx: " << mgr.get_err_msg() << std::endl;
    retcode = 1;
  }
  return retcode;
}
