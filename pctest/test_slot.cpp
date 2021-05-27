#include <pc/manager.hpp>
#include <pc/log.hpp>
#include <unistd.h>
#include <iostream>

using namespace pc;

class manager_slot : public manager
{
public:
  void on_response( rpc::slot_subscribe * ) override;
};

void manager_slot::on_response( rpc::slot_subscribe *res )
{
  manager::on_response( res );
  std::cout << get_curr_time()
            << ','
            << res->get_slot()
            << std::endl;
}

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
  std::cerr << "usage: test_slot [options]" << std::endl;
  std::cerr << "options include:" << std::endl;
  std::cerr << "  -r <rpc_host (default " << get_rpc_host() << ")>"
             << std::endl;
  std::cerr << "     Host name or IP address of solana rpc node in the form "
               "host_name[:rpc_port[:ws_port]]\n" << std::endl;
  std::cerr << "  -k <key_store_directory (default "
            << get_key_store() << ">" << std::endl;
  std::cerr << "     Directory name housing publishing, mapping and program"
               " key files\n" << std::endl;
  return 1;
}

int main( int argc, char **argv )
{
  // get input parameters
  int opt = 0;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "r:k:dh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      default: return usage();
    }
  }

  // initialize connection to block-chain
  manager_slot mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  mgr.set_do_tx( false );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "test_slot: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  while( !mgr.get_is_err() ) {
    mgr.poll();
  }
  if ( mgr.get_is_err() ) {
    std::cerr << "test_slot: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  return 0;
}
