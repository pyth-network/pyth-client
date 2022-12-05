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

std::string get_tx_host()
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
  std::cerr << "     Host name or IP address of solana rpc node in the form "
               "host_name[:rpc_port[:ws_port]]\n" << std::endl;
  std::cerr << "  -t <tx proxy host (default " << get_tx_host() << ")>"
            << std::endl;
  std::cerr << "     Host name or IP address of running pyth_tx server\n"
            << std::endl;
  std::cerr << "  -i <interval with which to send notify_price_sched notifications, in milliseconds (default 1000) >" << std::endl;
  std::cerr << "  -k <key_store_directory (default " << get_key_store()
            << ")>]" << std::endl;
  std::cerr << "     Directory name housing publishing, mapping and program"
               " key files\n" << std::endl;
  std::cerr << "  -p <listening_port (default " << get_port() << ">"
            << std::endl;
  std::cerr << "     Websocket port number for clients to connect to\n"
            << std::endl;
  std::cerr << "  -w <web content directory>" << std::endl;
  std::cerr << "     Directory containing dashboard/ content\n" << std::endl;
  std::cerr << "  -c <capture file>" << std::endl;
  std::cerr << "     Optional capture will get compressed\n" << std::endl;
  std::cerr << "  -l <log_file>" << std::endl;
  std::cerr << "     Optional log file - uses stderr if not provided\n"
            << std::endl;
  std::cerr << "  -n" << std::endl;
  std::cerr << "     No wait mode - i.e. run using busy poll loop\n"
            << std::endl;
  std::cerr << "  -x" << std::endl;
  std::cerr << "     Disable connection to pyth_tx transaction proxy server"
               "\n" << std::endl;
  std::cerr << "  -z" << std::endl;
  std::cerr << "     Disable WebSocket connection to Solana RPC node"
               "\n" << std::endl;
  std::cerr << "  -m <commitment_level>" << std::endl;
  std::cerr << "     Subscription commitment level: processed, confirmed or "
               "finalized\n" << std::endl;
  std::cerr << "  -d" << std::endl;
  std::cerr << "     Turn on debug logging. Can also toggle this on/off via "
               "kill -s SIGUSR1 <pid>\n" << std::endl;
  std::cerr << "  -u" << std::endl;
  std::cerr << "     Number of compute units requested by each upd_price transaction (default 20000)" << std::endl;
  std::cerr << "  -v" << std::endl;
  std::cerr << "     Price per compute unit for each upd_price transaction, in micro lamports (the default is not to specify a specific price)" << std::endl;
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
  commitment cmt = commitment::e_confirmed;
  std::string cnt_dir, cap_file, log_file;
  std::string rpc_host = get_rpc_host();
  std::string secondary_rpc_host = "";
  std::string key_dir  = get_key_store();
  std::string tx_host  = get_tx_host();
  int pyth_port = get_port();
  int opt = 0;
  int pub_int = 1000;
  unsigned cu_units = 20000;
  unsigned cu_price = 0;
  unsigned max_batch_size = 0;
  bool do_wait = true, do_tx = true, do_ws = true, do_debug = false;
  while( (opt = ::getopt(argc,argv, "r:s:t:p:i:k:w:c:l:m:b:u:v:dnxhz" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 's': secondary_rpc_host = optarg; break;
      case 't': tx_host = optarg; break;
      case 'p': pyth_port = ::atoi(optarg); break;
      case 'i': pub_int = ::atoi(optarg); break;
      case 'k': key_dir = optarg; break;
      case 'c': cap_file = optarg; break;
      case 'w': cnt_dir = optarg; break;
      case 'l': log_file = optarg; break;
      case 'm': cmt = str_to_commitment(optarg); break;
      case 'b': max_batch_size = strtoul(optarg, NULL, 0); break;
      case 'n': do_wait = false; break;
      case 'x': do_tx = false; break;
      case 'z': do_ws = false; break;
      case 'd': do_debug = true; break;
      case 'u': cu_units = strtoul(optarg, NULL, 0); break;
      case 'v': cu_price = strtoul(optarg, NULL, 0); break;
      default: return usage();
    }
  }
  if ( cmt == commitment::e_unknown ) {
    std::cerr << "pythd: unknown commitment level" << std::endl;
    return usage();
  }

  // set up logging and disable SIGPIPE
  signal( SIGPIPE, SIG_IGN );
  if ( !log_file.empty() && !log::set_log_file( log_file ) ) {
    std::cerr << "pythd: failed to create log_file="
              << log_file << std::endl;
    return 1;
  }
  log::set_level( do_debug ? PC_LOG_DBG_LVL : PC_LOG_INF_LVL );

  // construct and initialize pyth-client manager
  manager mgr;
  mgr.set_dir( key_dir );
  mgr.set_rpc_host( rpc_host );
  mgr.set_tx_host( tx_host );
  mgr.set_listen_port( pyth_port );
  mgr.set_content_dir( cnt_dir );
  mgr.set_capture_file( cap_file );
  mgr.set_do_tx( do_tx );
  mgr.set_do_ws( do_ws );
  mgr.set_do_capture( !cap_file.empty() );
  mgr.set_commitment( cmt );
  mgr.set_publish_interval( pub_int );
  mgr.set_requested_upd_price_cu_units( cu_units );
  mgr.set_requested_upd_price_cu_price( cu_price );

  bool do_secondary = !secondary_rpc_host.empty();
  if ( do_secondary ) {
    mgr.add_secondary( secondary_rpc_host, key_dir );
  }
  if ( !mgr.init() ) {
    std::cerr << "pythd: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  if ( !mgr.get_mapping_pub_key() ) {
    std::cerr << "pythd: missing mapping account key ["
      << mgr.get_mapping_pub_key_file() << "]" << std::endl;
    return 1;
  }

  if (max_batch_size > 0) {
    mgr.set_max_batch_size(max_batch_size);
  }
  std::cout << "pythd: max batch size " << mgr.get_max_batch_size() << std::endl;

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
    std::cerr << "pythd: " << mgr.get_err_msg() << std::endl;
    retcode = 1;
  }
  return retcode;
}
