#include <pc/manager.hpp>
#include <pc/log.hpp>
#include <unistd.h>
#include <iostream>

using namespace pc;

#define PC_LEADER_MAX         256
#define PC_LEADER_MIN         32

class manager_slot : public manager,
                     public rpc_sub_i<rpc::get_slot_leaders>
{
public:
  manager_slot();
  void on_response( rpc::slot_subscribe * ) override;
  void on_response( rpc::get_slot_leaders * ) override;
private:
  rpc::get_slot_leaders ldr_[1];
  uint64_t              last_;
};

manager_slot::manager_slot()
: last_( 0L )
{
  ldr_->set_sub( this );
  ldr_->set_limit( PC_LEADER_MAX );
}

void manager_slot::on_response( rpc::slot_subscribe *res )
{
  manager::on_response( res );
  uint64_t slot = get_slot();
  if ( slot != last_ ) {
    // request next slot leader schedule
    if ( PC_UNLIKELY( ldr_->get_is_recv() &&
                      slot > ldr_->get_last_slot() - PC_LEADER_MIN ) ) {
      ldr_->set_slot( slot );
      get_rpc_client()->send( ldr_ );
    }

    // ignore first time
    if ( !last_ ) {
      last_ = slot;
      return;
    }

    // get leader for this slot
    std::string pstr;
    pub_key *pkey = ldr_->get_leader( slot );
    if ( pkey ) {
      pkey->enc_base58( pstr );
    }
    std::cout << get_curr_time()
              << ','
              << slot
              << ','
              << pstr
              << std::endl;
    last_ = slot;
  }
}

void manager_slot::on_response( rpc::get_slot_leaders *m )
{
  if ( m->get_is_err() ) {
    set_err_msg( "failed to get slot leaders ["
        + m->get_err_msg()  + "]" );
    return;
  }
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
  std::cerr << "usage: slots_info [options]" << std::endl;
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
  std::cout << "recv_time,slot,leader" << std::endl;
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "slots_info: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  while( !mgr.get_is_err() ) {
    mgr.poll();
  }
  if ( mgr.get_is_err() ) {
    std::cerr << "slots_info: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  return 0;
}
