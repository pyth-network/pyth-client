#include <pc/rpc_client.hpp>
#include <iostream>

using namespace pc;

class test_rpc_sub : public rpc_sub,
                     public rpc_sub_i<rpc::get_account_info>,
                     public rpc_sub_i<rpc::get_recent_block_hash>
{
public:
  test_rpc_sub() : cnt_( 0 ) {
  }
  void add() {
    ++cnt_;
  }
  void on_response( rpc::get_account_info *m ) {
    size_t clen = 0;
    const char *cptr;
    std::cout << "===============> get_account_info" << std::endl;
    std::cout << "slot          = " << m->get_slot() << std::endl;
    std::cout << "lamports      = " << m->get_lamports() << std::endl;
    std::cout << "rent_epoch    = " << m->get_rent_epoch() << std::endl;
    std::cout << "is_executable = " << m->get_is_executable() << std::endl;
    std::cout << "owner         = ";
    m->get_owner( cptr, clen );
    std::cout.write( cptr, clen );
    std::cout << std::endl << std::endl;
    --cnt_;
  }
  void on_response( rpc::get_recent_block_hash *m ) {
    std::cout << "===============> get_recent_block_hash" << std::endl;
    std::cout << "slot              = " << m->get_slot() << std::endl;
    std::cout << "lamports_per_sig  = "
              << m->get_lamports_per_signature() << std::endl;
    hash bhash = m->get_block_hash();
    std::string hstr;
    bhash.enc_base58( hstr );
    std::cout << "recent_block_hash = " << hstr << std::endl;
    std::cout << std::endl;
    --cnt_;
  }
  bool is_done() const {
    return cnt_ == 0;
  }
private:
  int *cnt_;
};

int main(int,char**)
{
  // connect to validor node rpc port
  tcp_connect conn;
  conn.set_host( "localhost" );
  conn.set_port( 8899 );

  rpc_client clnt;
  clnt.set_http_conn( &conn );

  if ( !conn.init() ) {
    std::cerr << "test_http : " << conn.get_err_msg() << std::endl;
    return 1;
  }
  std::cout << "connected" << std::endl;

  // get account key
  static const char kptxt[] = "[1,255,171,208,173,142,62,253,217,43,175,186,121,205,69,158,81,20,106,216,112,153,91,128,111,144,115,208,226,228,180,230,54,224,118,105,238,95,215,221,52,118,41,49,241,73,160,221,225,36,45,167,11,203,7,232,201,166,138,219,218,113,232,229 ]";
  key_pair kp;
  kp.init_from_json( kptxt );
  pub_key pk( kp );

  test_rpc_sub sub;

  // construct get_account_info request
  rpc::get_account_info req1;
  req1.set_account( pk );
  req1.set_sub( &sub );
  clnt.send( &req1 );
  sub.add();

  // construct get_recent_block_hash request
  rpc::get_recent_block_hash req2;
  req2.set_sub( &sub );
  clnt.send( &req2 );
  sub.add();

  // submit request and poll for result
  while( !conn.get_is_err() && !sub.is_done() ) {
    conn.poll();
  }

  return 0;
}
