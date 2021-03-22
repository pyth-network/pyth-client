#include <pc/manager.hpp>
#include <pc/log.hpp>
#include <unistd.h>
#include <signal.h>
#include <iostream>

// pyth daemon service

using namespace pc;

std::string get_rpc_host()
{
  return "localhost";
}

std::string get_key_store()
{
  std::string dir = getenv("HOME");
  return dir + "/.pythd/";
}

int get_port()
{
  return 8910;
}

int usage()
{
  std::cerr << "usage: pythd [options]" << std::endl << std::endl;
  std::cerr << "options include:" << std::endl;
  std::cerr << "  -r <rpc_host (default " << get_rpc_host() << ")>"
            << std::endl;
  std::cerr << "  -k <key_store_directory (default " << get_key_store()
            << ")>]" << std::endl;
  std::cerr << "  -p <listening_port (default " << get_port() << ">"
            << std::endl;
  std::cerr << "  -d"
            << std::endl;
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
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  int pyth_port = get_port();
  int opt = 0;
  while( (opt = ::getopt(argc,argv, "r:p:k:dh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'p': pyth_port = ::atoi(optarg); break;
      case 'k': key_dir = optarg; break;
      default: return usage();
    }
  }

  // set up logging and disable SIGPIPE
  signal( SIGPIPE, SIG_IGN );
  log::set_level( PC_LOG_INF_LVL );

  // construct and initialize pyth-client manager
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_listen_port( pyth_port );
  mgr.set_dir( key_dir );
  if ( !mgr.init() ) {
    std::cerr << "pythd: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  if ( !mgr.get_mapping_pub_key() ) {
    std::cerr << "pythd: missing mapping account key ["
      << mgr.get_mapping_pub_key_file() << "]" << std::endl;
    return 1;
  }
  // set up signal handing
  signal( SIGINT, sig_handle );
  signal( SIGHUP, sig_handle );
  signal( SIGTERM, sig_handle );
  signal( SIGUSR1, sig_toggle );
  while( do_run && !mgr.get_is_err() ) {
    mgr.poll();
  }
  int retcode = 0;
  if ( mgr.get_is_err() ) {
    std::cerr << "pythd: " << mgr.get_err_msg() << std::endl;
    retcode = 1;
  }
  return retcode;
}
