#include <pc/pyth_client.hpp>
#include <pc/log.hpp>
#include <iostream>

using namespace pc;

int main(int,char**)
{
  log::set_level( PC_LOG_DBG_LVL );

  // get params
  key_pair sender;
  if (!sender.init_from_file( "/home/richard/test_key_1.json" ) ){
    std::cerr << "failed to find sender" << std::endl;
    return 1;
  }
  key_pair account;
  if ( !account.init_from_file( "/home/richard/account_1.json" ) ) {
    std::cerr << "failed to find account" << std::endl;
    return 1;
  }
  key_pair prog_id;
  if ( !prog_id.init_from_file( "/home/richard/program-id.json" ) ) {
    std::cerr << "failed to find program-id" << std::endl;
    return 1;
  }
  uint64_t funds = 5000000000L; // 5 SOL
  uint64_t space = 1024;

  // construct connection
  pyth_server psvr;
  psvr.set_rpc_host( "localhost" );
  psvr.set_listen_port( 8910 );
  psvr.set_key_file( "/home/richard/test_key_1.json" );
  if ( !psvr.init() ) {
    std::cerr << "pythd: " << psvr.get_err_msg() << std::endl;
    return 1;
  }
  // construct request
  pyth::create_mapping req;
  rpc::create_account *rptr = req.get_create_account();
  rptr->set_sender( sender );
  rptr->set_account( account );
  rptr->set_owner( prog_id );
  rptr->set_lamports( funds );
  rptr->set_space( space );

  // submit request and poll for results
  psvr.submit( &req );
  for(;;) {
    psvr.poll();
  }

  return 0;
}
