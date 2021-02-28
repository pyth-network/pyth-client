#include <pc/pyth_client.hpp>
#include <unistd.h>
#include <signal.h>
#include <iostream>

// pyth daemon service

using namespace pc;

int usage()
{
  std::cerr << "usage: pythd:"
            << " [-r <rpc_host>]"    // remote (or local) validator node
            << " [-l <listen_addr>]" // listen tcp or domain sockets
            << " [-p <proxy_addr>"   // run in proxy mode and fwd requests
            << " [-k <key_file>]"    // key_pair file in solana format
            << " [-d]"               // daemonize
            << std::endl;
  // listen address: localhost, localhost:8080, eth1:9809,
  //   :8080, my_host, /var/run/pythd
  return 1;
}

bool do_run = true;

void sig_handle( int )
{
  do_run = false;
}

int main(int argc, char **argv)
{
  // command-line parsing
  std::string rpc_host = "localhost";
  int pyth_port = 8910;
  int opt = 0;
  while( (opt = ::getopt(argc,argv, "r:l:p:k:dh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'l': pyth_port = ::atoi(optarg); break;
      default: return usage();
    }
  }

  // set up signal handing
  signal( SIGINT, sig_handle );
  signal( SIGHUP, sig_handle );
  signal( SIGTERM, sig_handle );
  signal( SIGPIPE, SIG_IGN );

  // construct and initialize pyth_server
  pyth_server psvr;
  psvr.set_rpc_host( rpc_host );
  psvr.set_listen_port( pyth_port );
  if ( !psvr.init() ) {
    std::cerr << "pythd: " << psvr.get_err_msg() << std::endl;
    return 1;
  }
  while( do_run ) {
    psvr.poll();
  }
  psvr.teardown();

  return 0;
}
