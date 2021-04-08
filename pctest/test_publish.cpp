#include <pc/manager.hpp>
#include <pc/log.hpp>
#include <unistd.h>
#include <signal.h>
#include <iostream>

// system-wide notification events
class test_connect : public pc::manager_sub
{
public:
  test_connect();

  // on connection to (but not initialization) solana validator
  void on_connect( pc::manager * ) override;

  // on disconnect from solana validator
  void on_disconnect( pc::manager * ) override;

  // on completion of (re)bootstrap of accounts following (re)connect
  void on_init( pc::manager * ) override;

  // on addition of new symbols
  void on_add_symbol( pc::manager *, pc::price * ) override;

  // have we received an on_init() callback yet
  bool get_is_init() const;

private:
  bool is_init_;
};

// subscriber callback implementation
class test_publish : public pc::request_sub,
                     public pc::request_sub_i<pc::price>,
                     public pc::request_sub_i<pc::price_sched>
{
public:
  test_publish( pc::price *sym, int64_t px, uint64_t sprd );
  ~test_publish();

  // callback for on-chain aggregate price update
  void on_response( pc::price*, uint64_t ) override;

  // callback for when to submit new price on-chain
  void on_response( pc::price_sched *, uint64_t ) override;

private:
  pc::request_sub_set sub_;   // request subscriptions for this object
  int64_t             px_;    // price to submit
  uint64_t            sprd_;  // confidence interval or bid-ask spread
  double              expo_;  // price exponent
  uint64_t            sid1_;  // subscription id for prices
  uint64_t            sid2_;  // subscription id for scheduling
};

test_publish::test_publish( pc::price *sym, int64_t px, uint64_t sprd )
: sub_( this ),
  px_( px ),
  sprd_( sprd )
{
  // add subscriptions for price updates from block chain
  sid1_ = sub_.add( sym );

  // add subscription for price scheduling
  sid2_ = sub_.add( sym->get_sched() );

  // get price exponent for this symbol
  int64_t expo = sym->get_price_exponent();
  for( expo_ = 1.; expo <0.; expo_ *= 0.1, expo++ );
}

test_connect::test_connect()
: is_init_( false )
{
}

void test_connect::on_connect( pc::manager * )
{
  PC_LOG_INF( "test_connect: connected" ).end();
}

void test_connect::on_disconnect( pc::manager * )
{
  PC_LOG_INF( "test_connect: disconnected" ).end();
}

void test_connect::on_init( pc::manager * )
{
  PC_LOG_INF( "test_connect: initialized" ).end();
  is_init_ = true;
}

void test_connect::on_add_symbol( pc::manager *, pc::price *sym )
{
  PC_LOG_INF( "test_connect: new symbol added" )
    .add( "symbol", *sym->get_symbol() )
    .add( "price_type", pc::price_type_to_str( sym->get_price_type() ) )
    .end();
}

bool test_connect::get_is_init() const
{
  return is_init_;
}


test_publish::~test_publish()
{
  sub_.del( sid1_ ); // unsubscribe price updates
  sub_.del( sid2_ ); // unsubscribe price schedule updates
}

void test_publish::on_response( pc::price *sym, uint64_t sub_id )
{
  if ( sym->get_is_err() ) {
    PC_LOG_ERR( "error receiving aggregate price" )
      .add( "err", sym->get_err_msg() )
      .end();
    return;
  }
  // received aggregate price update for this symbol
  double price  = expo_ * (double)sym->get_price();
  double spread = expo_ * (double)sym->get_conf();
  PC_LOG_INF( "received aggregate price update" )
    .add( "symbol", *sym->get_symbol() )
    .add( "price_type", pc::price_type_to_str( sym->get_price_type() ) )
    .add( "agg_price", price )
    .add( "agg_spread", spread )
    .add( "valid_slot", sym->get_valid_slot() )
    .add( "pub_slot", sym->get_pub_slot() )
    .add( "sub_id", sub_id )
    .end();
}

void test_publish::on_response( pc::price_sched *ptr, uint64_t sub_id )
{
  // submit next price to block chain for this symbol
  pc::price *sym = ptr->get_price();
  if ( !sym->update( px_, sprd_, pc::symbol_status::e_trading ) ) {
    PC_LOG_ERR( "failed to submit price" )
      .add( "symbol", *sym->get_symbol() )
      .add( "price_type", pc::price_type_to_str( sym->get_price_type() ) )
      .add( "err_msg", sym->get_err_msg() )
      .end();
  } else {
    double price  = expo_ * (double)px_;
    double spread = expo_ * (double)sprd_;
    PC_LOG_INF( "submit price to block-chain" )
      .add( "symbol", *sym->get_symbol() )
      .add( "price_type", pc::price_type_to_str( sym->get_price_type() ) )
      .add( "price", price )
      .add( "spread", spread )
      .add( "slot", sym->get_manager()->get_slot() )
      .add( "sub_id", sub_id )
      .end();
    // increase price
    px_ += sprd_;
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

bool do_run = true;

void sig_handle( int )
{
  do_run = false;
}

int usage()
{
  std::cerr << "usage: test_publish " << std::endl;
  std::cerr << "  [-r <rpc_host (default " << get_rpc_host() << ")>]"
             << std::endl;
  std::cerr << "  [-k <key_store_directory (default "
            << get_key_store() << ">]" << std::endl;
  std::cerr << "  [-c <capture file>]" << std::endl;
  std::cout << "  [-l <log_file>]" << std::endl;
  std::cerr << "  [-n]" << std::endl;
  std::cerr << "  [-d]" << std::endl;
  return 1;
}

int main(int argc, char** argv)
{
  // unpack options
  int opt = 0;
  bool do_wait = true, do_debug = false;
  std::string cap_file, log_file;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "r:k:c:l:ndh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'c': cap_file = optarg; break;
      case 'l': log_file = optarg; break;
      case 'n': do_wait = false; break;
      case 'd': do_debug = true; break;
      default: return usage();
    }
  }
  // set logging level and disable SIGPIPE
  signal( SIGPIPE, SIG_IGN );
  if ( !log_file.empty() && !pc::log::set_log_file( log_file ) ) {
    std::cerr << "test_publish: failed to create log_file="
              << log_file << std::endl;
    return 1;
  }
  pc::log::set_level( do_debug ? PC_LOG_DBG_LVL : PC_LOG_INF_LVL );

  // set up signal handing (ignore SIGPIPE - its evil)
  signal( SIGINT, sig_handle );
  signal( SIGHUP, sig_handle );
  signal( SIGTERM, sig_handle );

  // callback fror system-wide events
  test_connect sub;

  // initialize connection to solana validator and bootstrap symbol list
  pc::manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  mgr.set_manager_sub( &sub );
  mgr.set_capture_file( cap_file );
  mgr.set_do_capture( !cap_file.empty() );
  if ( !mgr.init() ) {
    std::cerr << "test_publish: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // wait to initialize
  while( do_run && !mgr.get_is_err() && !sub.get_is_init() ) {
    mgr.poll( do_wait );
  }
  if ( mgr.get_is_err() || !do_run ) {
    std::cerr << "test_publish: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // get two symbols that we want to publish/subscribe
  pc::price *sym1 = mgr.get_symbol(
      "US.EQ.SYMBOL1", pc::price_type::e_price );
  if ( !sym1 ) {
    std::cerr << "test_publish: failed to find symbol1" << std::endl;
    return 1;
  }
  pc::price *sym2 = mgr.get_symbol(
      "US.EQ.SYMBOL2", pc::price_type::e_price );
  if ( !sym2 ) {
    std::cerr << "test_publish: failed to find symbol2" << std::endl;
    return 1;
  }

  // construct publisher/subscriber instances
  test_publish pub1( sym1, 10000, 100 );
  test_publish pub2( sym2, 2000000, 20000 );

  // run event loop and wait for price updates and requests to submit price
  while( do_run && !mgr.get_is_err() ) {
    mgr.poll( do_wait );
  }

  // report any errors on exit
  // please note that manager exits in error if error submitting price
  int retcode = 0;
  if ( mgr.get_is_err() ) {
    std::cerr << "test_publish: " << mgr.get_err_msg() << std::endl;
    retcode = 1;
  }
  return retcode;
}

