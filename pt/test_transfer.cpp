#include <pc/rpc_client.hpp>
#include <iostream>

using namespace pc;

class test_transfer : public rpc::transfer,
                      public rpc_sub,
                      public rpc_sub_i<rpc::transfer>,
                      public rpc_sub_i<rpc::get_recent_block_hash>,
                      public rpc_sub_i<rpc::signature_subscribe>
{
public:
  test_transfer( rpc_client *conn ) : conn_( conn ), done_( false ) {
    req_.set_sub( this );
    sig_.set_sub( this );
    set_sub( this );
  }
  void on_response( rpc::get_recent_block_hash *m ) {
    // send transfer
    set_block_hash( m->get_block_hash() );
    conn_->send( this );
    // subscribe to transfer completion
    sig_.set_signature( get_signature() );
    conn_->send( &sig_ );
  }
  void on_response( rpc::transfer * ) {
  }
  void on_response( rpc::signature_subscribe *sig ) {
    if ( !sig->get_is_err() ) {
      std::cout << "finished transaction" << std::endl;
    } else {
      std::cerr << "error:" << sig->get_err_msg() << std::endl;
    }
    done_ = true;
  }
  bool is_done() const {
    return done_;
  }
  void send() {
    conn_->send( &req_ );
  }
private:
  rpc_client *conn_;
  bool        done_;
  rpc::get_recent_block_hash req_;
  rpc::signature_subscribe   sig_;
};

int main(int,char**)
{
  // validator node rpc port
  tcp_connect hconn;
  hconn.set_host( "localhost" );
  hconn.set_port( 8899 );

  // validator node websocket port
  ws_connect wconn;
  wconn.set_host( "localhost" );
  wconn.set_port( 8900 );

  // rpc request manager
  rpc_client clnt;
  clnt.set_http_conn( &hconn );
  clnt.set_ws_conn( &wconn );

  // initialize connections
  if ( !hconn.init() ) {
    std::cerr << "test_transfer : " << hconn.get_err_msg() << std::endl;
    return 1;
  }
  if ( !wconn.init() ) {
    std::cerr << "test_transfer : " << wconn.get_err_msg() << std::endl;
    return 1;
  }
  std::cout << "connected" << std::endl;

  // get sender, receiver and amount parameters
  static const char sndtxt[] = "[1,255,171,208,173,142,62,253,217,43,175,186,121,205,69,158,81,20,106,216,112,153,91,128,111,144,115,208,226,228,180,230,54,224,118,105,238,95,215,221,52,118,41,49,241,73,160,221,225,36,45,167,11,203,7,232,201,166,138,219,218,113,232,229 ]";
  key_pair snd;
  snd.init_from_json( sndtxt );

  static const char rcvtxt[] = "[250,253,9,53,225,31,214,240,1,58,97,59,148,137,248,39,38,220,19,101,158,220,152,182,205,138,178,151,170,84,184,192,106,21,194,67,236,136,201,96,190,168,115,189,162,65,74,99,163,119,180,155,55,116,174,138,226,204,57,62,59,110,208,88]";
  key_pair rcv;
  rcv.init_from_json( rcvtxt );
  pub_key  rcv_pub( rcv );
  long amt = 5000000000L; // 5 SOL

  // construct and submit transfer request
  test_transfer req( &clnt );
  req.set_sender( snd );
  req.set_receiver( rcv_pub );
  req.set_lamports( amt );
  req.send();

  // submit request and poll for result
  while( !hconn.get_is_err() &&
         !wconn.get_is_err() &&
         !clnt.get_is_err() &&
         !req.is_done() ) {
    hconn.poll();
    wconn.poll();
  }

  // report errors
  if ( wconn.get_is_err() ) {
    std::cerr << "test_transfer: " << wconn.get_err_msg() << std::endl;
    return 1;
  }
  if ( hconn.get_is_err() ) {
    std::cerr << "test_transfer: " << hconn.get_err_msg() << std::endl;
    return 1;
  }

  return 0;
}
