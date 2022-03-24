#include <pc/manager.hpp>
#include <pc/log.hpp>
#include <unistd.h>
#include <signal.h>
#include <iostream>
#include <stdlib.h>

class test_publish;

// system-wide notification events and factory for publisher
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

  // on connection to pyth_tx proxy publisher
  void on_tx_connect( pc::manager * ) override;

  // on disconnect from pyth_tx proxy publisher
  void on_tx_disconnect( pc::manager * ) override;

  // construct publishers on addition of new symbols
  void on_add_symbol( pc::manager *, pc::price * ) override;

  // when we receive a new slot
  void on_slot_publish( pc::manager * ) override;

  // have we received an on_init() callback yet
  bool get_is_init() const;

  void teardown();

  test_publish *pub1_;     // SYMBOL1 publisher
  test_publish *pub2_;     // SYMBOL2 publisher
};

// subscriber callback implementation
class test_publish : public pc::request_sub,
                     public pc::request_sub_i<pc::product>,
                     public pc::request_sub_i<pc::price>,
                     public pc::request_sub_i<pc::price_init>
{
public:
  test_publish( pc::price *sym, int64_t px, uint64_t sprd );
  ~test_publish();

  // callback for on-chain product reference data update
  void on_response( pc::product*, uint64_t ) override;

  // callback for on-chain aggregate price update
  void on_response( pc::price*, uint64_t ) override;

  // callback for re-initialization of price account (with diff. exponent)
  void on_response( pc::price_init *, uint64_t ) override;

  // user-facing method to update the price this publisher will send
  void update_price( int64_t px_, uint64_t sprd_ );

  // the pyth price publisher for this symbol
  pc::price *sym_;

  // indicates if there is an update pending which should be sent in the next slot
  bool has_update_pending;

private:
  void unsubscribe();

  pc::request_sub_set sub_;   // request subscriptions for this object
  int64_t             px_;    // price to submit
  uint64_t            sprd_;  // confidence interval or bid-ask spread
  double              expo_;  // price exponent
  uint64_t            sid1_;  // subscription id for prices
  uint64_t            sid2_;  // subscription id for products
  uint64_t            rcnt_;  // price receive count
};

test_publish::test_publish( pc::price *sym, int64_t px, uint64_t sprd )
: sym_( sym ),
  has_update_pending( false ),
  sub_( this ),
  px_( px ),
  sprd_( sprd ),
  rcnt_( 0UL )
{
  // add subscriptions for price updates from block chain
  sid1_ = sub_.add( sym );

  // add subscription for product updates
  sid2_ = sub_.add( sym->get_product() );

  // get price exponent for this symbol
  int64_t expo = sym->get_price_exponent();
  for( expo_ = 1.; expo <0.; expo_ *= 0.1, expo++ );
}

test_connect::test_connect()
: pub1_( nullptr ),
  pub2_( nullptr )
{
}

void test_connect::teardown()
{
  delete pub1_;
  delete pub2_;
  pub1_ = pub2_ = nullptr;
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
}

void test_connect::on_tx_connect( pc::manager * )
{
  PC_LOG_INF( "test_connect: pyth_tx connected" ).end();
}

void test_connect::on_tx_disconnect( pc::manager * )
{
  PC_LOG_INF( "test_connect: pyth_tx disconnected" ).end();
}

void test_connect::on_add_symbol( pc::manager *, pc::price *sym )
{
  // gnore non-price quote types
  if ( sym->get_price_type() != pc::price_type::e_price ) {
    return;
  }
  PC_LOG_INF( "test_connect: new symbol added" )
    .add( "symbol", sym->get_symbol() )
    .add( "price_type", pc::price_type_to_str( sym->get_price_type() ) )
    .end();

  // construct publishers based on nasdaq symbol
  pc::str nsym;
  if ( !sym->get_attr( pc::attr_id( "nasdaq_symbol"), nsym ) ) {
    // not a product with a nasdaq symbol
    return;
  }
  // construct publisher for SYMBOL1
  if ( nsym == "SYMBOL1" && !pub1_ ) {
    pub1_ = new test_publish( sym, 10000, 100 );
  }
  // construct publisher for SYMBOL2
  if ( nsym == "SYMBOL2" && !pub2_ ) {
    pub2_ = new test_publish( sym, 2000000, 20000 );
  }

  // iterate through all the product attributes and log them
  pc::product *prod = sym->get_product();
  pc::str val_str;
  for( pc::attr_id id; prod->get_next_attr( id, val_str ); ) {
    PC_LOG_INF( prod->get_symbol() )
      .add( id.get_str(), val_str )
      .end();
  }
}

// Send any pending updates when a new slot is published.
void test_connect::on_slot_publish( pc::manager * )
{
  // Collect all the prices that are pending updates
  std::vector<pc::price*> updates;
  if ( pub1_ && pub1_->has_update_pending ) {
    updates.emplace_back( pub1_->sym_ );
  }
  if ( pub2_ && pub2_->has_update_pending ) {
    updates.emplace_back( pub2_->sym_ );
  }
  
  // Do nothing if there are no pending updates
  if ( updates.empty() ) {
    return;
  }

  // Send the batch price update
  if ( !pc::price::send( updates.data(), updates.size()) ) {
    PC_LOG_ERR( "batch send failed" ).end();
  }

  // Mark the updates as completed
  updates.clear();
  pub1_->has_update_pending = false;
  pub2_->has_update_pending = false;
}

test_publish::~test_publish()
{
  unsubscribe();
}

void test_publish::unsubscribe()
{
  // unsubscribe to callbacks
  sub_.del( sid1_ ); // unsubscribe price updates
}

void test_publish::on_response( pc::product *prod, uint64_t )
{
  PC_LOG_INF( "product ref. data update" )
    .add( "symbol", prod->get_symbol() )
    .end();

  // iterate through all the product attributes and log them
  pc::str val_str;
  for( pc::attr_id id; prod->get_next_attr( id, val_str ); ) {
    PC_LOG_INF( prod->get_symbol() )
      .add( id.get_str(), val_str )
      .end();
  }
}

void test_publish::on_response( pc::price *sym, uint64_t )
{
  // check if currently in error
  if ( sym->get_is_err() ) {
    PC_LOG_ERR( "error receiving aggregate price" )
      .add( "err", sym->get_err_msg() )
      .end();
    unsubscribe();
    return;
  }

  // get my own contribution to the aggregate
  pc::symbol_status my_status = pc::symbol_status::e_unknown;
  double  my_price = 0., my_conf = 0.;
  uint64_t my_slot = 0UL;
  pc::pub_key *my_key = sym->get_manager()->get_publish_pub_key();
  for(unsigned i=0; i !=  sym->get_num_publisher(); ++i ) {
    const pc::pub_key *ikey = sym->get_publisher( i );
    if ( *my_key == *ikey ) {
      my_price  = expo_ * (double)sym->get_publisher_price( i );
      my_conf   = expo_ * (double)sym->get_publisher_conf( i );
      my_slot   = sym->get_publisher_slot( i );
      my_status = sym->get_publisher_status( i );
      break;
    }
  }

  // received aggregate price update for this symbol
  double price  = expo_ * (double)sym->get_price();
  double spread = expo_ * (double)sym->get_conf();
  PC_LOG_INF( "received aggregate price update" )
    .add( "symbol", sym->get_symbol() )
    .add( "price_type", pc::price_type_to_str( sym->get_price_type() ) )
    .add( "status", pc::symbol_status_to_str( sym->get_status() ) )
    .add( "agg_price", price )
    .add( "agg_spread", spread )
    .add( "twap", expo_*(double)sym->get_twap() )
    .add( "twac", expo_*(double)sym->get_twac() )
    .add( "num_qt", sym->get_num_qt() )
    .add( "valid_slot", sym->get_valid_slot() )
    .add( "pub_slot", sym->get_pub_slot() )
    .add( "my_price", my_price )
    .add( "my_conf", my_conf )
    .add( "my_status", pc::symbol_status_to_str( my_status ) )
    .add( "my_slot", my_slot )
    .end();

  // periodically log publish statistics
  if ( ++rcnt_ % 10 == 0 ) {
    uint32_t slot_quartiles[4];
    sym->get_slot_quartiles( slot_quartiles );
      PC_LOG_INF( "publish statistics" )
        .add( "symbol", sym->get_symbol() )
        .add( "price_type", pc::price_type_to_str( sym->get_price_type() ) )
        .add( "num_agg", sym->get_num_agg() )
        .add( "num_sent", sym->get_num_sent() )
        .add( "num_recv", sym->get_num_recv() )
        .add( "num_sub_drop", sym->get_num_sub_drop() )
        .add( "hit_rate", sym->get_hit_rate() )
        .add( "slot_p25", slot_quartiles[0] )
        .add( "slot_p50", slot_quartiles[1] )
        .add( "slot_p75", slot_quartiles[2] )
        .add( "slot_p99", slot_quartiles[3] )
        .end();
  }
}

void test_publish::on_response( pc::price_init *ptr, uint64_t )
{
  pc::price *sym = ptr->get_price();
  PC_LOG_INF( "price account change" )
    .add( "symbol", sym->get_symbol() )
    .add( "exponent", sym->get_price_exponent() )
    .end();
}

// Updates the price value stored locally, without sending the update.
void test_publish::update_price( int64_t px_, uint64_t sprd_ ) {
  sym_->update_no_send( px_, sprd_, pc::symbol_status::e_trading, false );
  has_update_pending = true;
}

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
  std::cerr << "  [-t <pyth_tx host (default " << get_tx_host() << ")>]"
             << std::endl;
  std::cerr << "  [-k <key_store_directory (default "
            << get_key_store() << ")>]" << std::endl;
  std::cerr << "  [-c <capture file>]" << std::endl;
  std::cout << "  [-l <log_file>]" << std::endl;
  std::cerr << "  [-m <commitment_level>]" << std::endl;
  std::cerr << "  [-n]" << std::endl;
  std::cerr << "  [-d]" << std::endl;
  std::cerr << "  [-x]" << std::endl;
  return 1;
}

int64_t random_value( )
{
  return rand() % 10 + 1;
}

int main(int argc, char** argv)
{
  // unpack options
  int opt = 0;
  pc::commitment cmt = pc::commitment::e_confirmed;
  bool do_wait = true, do_debug = false, do_tx = true;
  std::string cap_file, log_file;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  std::string tx_host  = get_tx_host();
  while( (opt = ::getopt(argc,argv, "r:t:k:c:l:m:ndxh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 't': tx_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'c': cap_file = optarg; break;
      case 'l': log_file = optarg; break;
      case 'm': cmt = pc::str_to_commitment(optarg); break;
      case 'n': do_wait = false; break;
      case 'd': do_debug = true; break;
      case 'x': do_tx = false; break;
      default: return usage();
    }
  }
  if ( cmt == pc::commitment::e_unknown ) {
    std::cerr << "pythd: unknown commitment level" << std::endl;
    return usage();
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
  mgr.set_tx_host( tx_host );
  mgr.set_do_tx( do_tx );
  mgr.set_dir( key_dir );
  mgr.set_manager_sub( &sub );
  mgr.set_capture_file( cap_file );
  mgr.set_do_capture( !cap_file.empty() );
  mgr.set_commitment( cmt );
  if ( !mgr.init() ) {
    std::cerr << "test_publish: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // run event loop and wait for product updates, price updates
  // and requests to submit price
  while( do_run && !mgr.get_is_err() ) {
    mgr.poll( do_wait );

    // Submit new price updates
    if ( sub.pub1_ != nullptr ) {
      sub.pub1_->update_price( random_value() ,  uint64_t( random_value() ) );
    }
    if ( sub.pub2_ != nullptr ) {
      sub.pub2_->update_price( random_value() ,  uint64_t( random_value() ) );
    }

  }

  // report any errors on exit
  // please note that manager exits in error if error submitting price
  int retcode = 0;
  if ( mgr.get_is_err() ) {
    std::cerr << "test_publish: " << mgr.get_err_msg() << std::endl;
    retcode = 1;
  }
  sub.teardown();

  return retcode;
}

