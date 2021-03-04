#include <pc/pyth_client.hpp>
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

int usage()
{
  std::cerr << "usage: pythd:"
            << " [-r <rpc_host>]"     // remote (or local) validator node
            << " [-p <listen_port>]"  // tcp listening port
            << " [-k <key_directory>]"// key store directory
            << " [-d]"                // daemonize
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
  int pyth_port = 8910;
  int opt = 0;
  while( (opt = ::getopt(argc,argv, "r:p:k:dh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'p': pyth_port = ::atoi(optarg); break;
      case 'k': key_dir = optarg; break;
      default: return usage();
    }
  }

  // set up signal handing
  signal( SIGINT, sig_handle );
  signal( SIGHUP, sig_handle );
  signal( SIGTERM, sig_handle );
  signal( SIGPIPE, SIG_IGN );
  signal( SIGUSR1, sig_toggle );
  log::set_level( PC_LOG_INF_LVL );

  // construct and initialize pyth_client
  pyth_client clnt;
  clnt.set_rpc_host( rpc_host );
  clnt.set_listen_port( pyth_port );
  clnt.set_dir( key_dir );
  if ( !clnt.init() ) {
    std::cerr << "pythd: " << clnt.get_err_msg() << std::endl;
    return 1;
  }
  if ( !clnt.get_mapping_pub_key() ) {
    std::cerr << "pythd: missing mapping account key ["
      << clnt.get_mapping_pub_key_file() << "]" << std::endl;
    return 1;
  }
  while( do_run ) {
    clnt.poll();
  }
  clnt.teardown();

  return 0;
}
