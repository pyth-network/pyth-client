#include <pc/rpc_client.hpp>
#include <iostream>

using namespace pc;

class get_account_info_sub : public rpc_sub,
                             public rpc_sub_i<rpc::get_account_info>
{
public:
  get_account_info_sub() : done_( false ) {
  }
  void on_response( rpc::get_account_info *m ) {
    size_t clen = 0;
    const char *cptr;
    std::cout << "slot          = " << m->get_slot() << std::endl;
    std::cout << "lamports      = " << m->get_lamports() << std::endl;
    std::cout << "rent_epoch    = " << m->get_rent_epoch() << std::endl;
    std::cout << "is_executable = " << m->get_is_executable() << std::endl;
    std::cout << "owner         = ";
    m->get_owner( cptr, clen );
    std::cout.write( cptr, clen );
    std::cout << std::endl;
    done_ = true;
  }
  bool is_done() {
    return done_;
  }
private:
  bool done_;
};

int main(int,char**)
{
  // connect to validor node rpc port
  rpc_client conn;
  tcp_connect clnt;
  clnt.set_host( "localhost" );
  clnt.set_port( 8899 );
  clnt.set_net_socket( &conn );
  if ( !clnt.init() ) {
    std::cerr << "test_http : " << clnt.get_err_msg() << std::endl;
    return 1;
  }
  conn.set_block( false );
  std::cout << "connected" << std::endl;

  // get account key
  static const char kptxt[] = "[1,255,171,208,173,142,62,253,217,43,175,186,121,205,69,158,81,20,106,216,112,153,91,128,111,144,115,208,226,228,180,230,54,224,118,105,238,95,215,221,52,118,41,49,241,73,160,221,225,36,45,167,11,203,7,232,201,166,138,219,218,113,232,229 ]";
  key_pair kp;
  kp.init_from_json( kptxt );
  pub_key pk( kp );

  get_account_info_sub sub;

  // construct request
  rpc::get_account_info req;
  req.set_account( pk );
  req.set_sub( &sub );

  // submit request and poll for result
  conn.send( &req );
  while( !conn.get_is_err() && !sub.is_done() ) {
    conn.poll();
  }

  return 0;
}
