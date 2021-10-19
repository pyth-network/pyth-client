#include <pc/manager.hpp>
#include <pc/log.hpp>

#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <iostream>

// pyth command-line tool

using namespace pc;

static const std::string DEFAULT_RPC_HOST  = "localhost";
static const std::string DEFAULT_TX_HOST   = "localhost";
static const std::string DEFAULT_KEY_STORE = std::string( getenv( "HOME" ) ) + "/.pythd/";

int usage()
{
  using namespace std;
  cerr << "usage: pyth" << endl;
  cerr << "  init_key         [options]" << endl;
  cerr << "  upd_price        <price_key> [options]" << endl;
  cerr << "  upd_price_val    <price_key> <value> <confidence> <status> [options]" << endl;
  cerr << "  get_product      <product_key> [options]" << endl;
  cerr << "  get_product_list [options]" << endl;
  cerr << "  get_all_products [options]" << endl;
  cerr << "  version" << endl;
  cerr << endl;

  cerr << "options include:" << endl;
  cerr << "  -r <rpc_host (default " << DEFAULT_RPC_HOST << ")>" << endl;
  cerr << "     Host name or IP address of solana rpc node in the form "
               "host_name[:rpc_port[:ws_port]]\n" << endl;
  cerr << "  -k <key_store_directory (default " << DEFAULT_KEY_STORE << ")>" << endl;
  cerr << "     Directory name housing publishing, mapping and program"
               " key files\n" << endl;
  cerr << "  -c <commitment_level (default confirmed)>" << endl;
  cerr << "     Options include processed, confirmed and finalized\n" << endl;
  cerr << "  -j\n"
            << "     Output results in json format where applicable\n" << endl;
  cerr << "  -d" << endl;
  cerr << "     Turn on debug logging\n" << endl;
  cerr << "  -h" << endl;
  cerr << "     Output this help text\n" << endl;

  cerr << "options only for upd_price / upd_price_val include:" << endl;
  cerr << "  -x" << endl;
  cerr << "     Disable connection to pyth_tx transaction proxy server\n" << endl;
  cerr << "  -t <tx proxy host (default " << DEFAULT_TX_HOST << ")>" << endl
            << "     Host name or IP address of running pyth_tx server" << endl;
  return 1;
}

struct pyth_arguments
{
  pyth_arguments( int argc, char **argv );

  bool         invalid_    = false;
  std::string  rpc_host_   = DEFAULT_RPC_HOST;
  std::string  tx_host_    = DEFAULT_TX_HOST;
  std::string  key_dir_    = DEFAULT_KEY_STORE;
  commitment   cmt_        = commitment::e_confirmed;
  bool         do_json_    = false;
  bool         do_tx_      = true;
};

pyth_arguments::pyth_arguments( int argc, char **argv )
{
  int opt = 0;
  while ( (opt = ::getopt( argc, argv, "r:t:k:c:jdxh" )) != -1 ) {
    switch (opt) {
      case 'r': rpc_host_ = optarg; break;
      case 't': tx_host_ = optarg; break;
      case 'k': key_dir_ = optarg; break;
      case 'c': cmt_ = str_to_commitment( optarg ); break;
      case 'j': do_json_ = true; break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      case 'x': do_tx_ = false; break;
      default:
        usage();
        invalid_ = true;
    }
  }

  if ( cmt_ == commitment::e_unknown ) {
    std::cerr << "pyth: unknown commitment level" << std::endl;
    invalid_ = true;
    usage();
  }
}

void print_val( str val, size_t sp=0 )
{
  static const char spaces[] =
  "                                                                          ";
  static const char dots[] =
  "..........................................................................";

  size_t num = 20 - sp;
  std::cout.write( spaces, static_cast< std::streamsize >( sp ) );
  std::cout.write( val.str_, static_cast< std::streamsize >( val.len_ ) );
  if ( num > val.len_ ) {
    std::cout.write( dots, static_cast< std::streamsize >( num - val.len_ ) );
  }
  std::cout << ' ';
}

int on_init_key( int argc, char **argv )
{
  pyth_arguments args( argc, argv );
  if ( args.invalid_ )
    return 1;

  key_store kst;
  kst.set_dir( args.key_dir_ );
  if ( !kst.create() ) {
    std::cerr << "pyth: " << kst.get_err_msg() << std::endl;
    return 1;
  }
  if ( !kst.init() ) {
    std::cerr << "pyth: " << kst.get_err_msg() << std::endl;
    return 1;
  }
  if ( kst.get_publish_key_pair() ) {
    std::cerr << "pyth: publish key pair already exists ["
      << kst.get_publish_key_pair_file() << "]" << std::endl;
    return 1;
  }
  if ( !kst.create_publish_key_pair() ) {
    std::cerr << "pyth: failed to create publish key pair ["
      << kst.get_publish_key_pair_file() << "]" << std::endl;
    std::cerr << "pyth: " << kst.get_err_msg() << std::endl;
    return 1;
  }

  return 0;
}

int on_upd_price( int argc, char **argv )
{
  if ( argc < 2 ) {
    return usage();
  }

  // Price Key
  std::string pkey( argv[1] );
  pub_key pub;
  pub.init_from_text( pkey );

  argc -= 1;
  argv += 1;

  pyth_arguments args( argc, argv );
  if ( args.invalid_ )
    return 1;

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( args.rpc_host_ );
  mgr.set_tx_host( args.tx_host_ );
  mgr.set_dir( args.key_dir_ );
  mgr.set_do_tx( args.do_tx_ );
  mgr.set_commitment( args.cmt_ );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  // get price and crank
  price *ptr = mgr.get_price( pub );
  if ( !ptr ) {
    std::cerr << "pyth: failed to find price key=" << pkey << std::endl;
    return 1;
  }
  if ( !ptr->has_publisher() ) {
    std::cerr << "pyth: missing publisher permission" << std::endl;
    return 1;
  }
  ptr->update();
  if ( args.do_tx_ ) {
    while( mgr.get_is_tx_send() && !mgr.get_is_err() )
      mgr.poll();
  } else {
    const auto send_ts = get_now();
    while( !mgr.get_is_err() &&
           !ptr->get_is_err() &&
           ( get_now() - send_ts < 15e9 ) &&
           ( mgr.get_is_rpc_send() || ptr->has_unacked_updates() ) )
      mgr.poll();
  }
  if ( ptr->get_is_err() ) {
    std::cerr << "pyth: " << ptr->get_err_msg() << std::endl;
    return 1;
  }
  if ( mgr.get_is_err() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  return 0;
}

int on_upd_price_val( int argc, char **argv )
{
  if ( argc < 5 ) {
    return usage();
  }

  // Price Key
  std::string pkey( argv[1] );
  pub_key pub;
  pub.init_from_text( pkey );

  // Price Value
  int64_t price_value = atoll( argv[2] );

  // Confidence
  int64_t confidence = atoll( argv[3] );
  assert( confidence >= 0 );

  // Status
  symbol_status price_status = str_to_symbol_status( argv[4] );

  // Make sure an unknown status was requested explicitly
  if ( price_status == symbol_status::e_unknown && argv[4] != std::string("unknown") ) {
    std::cerr << "pyth: unrecognized symbol status " << '"' << argv[4] << '"' << std::endl;
    return 1;
  }

  argc -= 4;
  argv += 4;

  pyth_arguments args( argc, argv );
  if ( args.invalid_ )
    return 1;

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( args.rpc_host_ );
  mgr.set_tx_host( args.tx_host_ );
  mgr.set_dir( args.key_dir_ );
  mgr.set_do_tx( args.do_tx_ );
  mgr.set_commitment( args.cmt_ );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  // get price and crank
  price *ptr = mgr.get_price( pub );
  if ( !ptr ) {
    std::cerr << "pyth: failed to find price key=" << pkey << std::endl;
    return 1;
  }
  if ( !ptr->has_publisher() ) {
    std::cerr << "pyth: missing publisher permission" << std::endl;
    return 1;
  }
  ptr->update(price_value, static_cast< uint64_t >( confidence ), price_status);
  if ( args.do_tx_ ) {
    while( mgr.get_is_tx_send() && !mgr.get_is_err() )
      mgr.poll();
  } else {
    const auto send_ts = get_now();
    while( !mgr.get_is_err() &&
           !ptr->get_is_err() &&
           ( get_now() - send_ts < 15e9 ) &&
           ( mgr.get_is_rpc_send() || ptr->has_unacked_updates() ) )
      mgr.poll();
  }
  if ( ptr->get_is_err() ) {
    std::cerr << "pyth: " << ptr->get_err_msg() << std::endl;
    return 1;
  }
  if ( mgr.get_is_err() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  return 0;
}

static void print_json( json_wtr& wtr )
{
  net_buf *hd, *tl;
  wtr.detach( hd, tl );
  for( net_buf *ptr = hd; ptr; ) {
    net_buf *nxt = ptr->next_;
    std::cout.write( ptr->buf_, ptr->size_ );
    ptr->dealloc();
    ptr = nxt;
  }
}

int on_get_product_list( int argc, char **argv )
{
  pyth_arguments args( argc, argv );
  if ( args.invalid_ )
    return 1;

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( args.rpc_host_ );
  mgr.set_dir( args.key_dir_ );
  mgr.set_do_tx( false );
  mgr.set_commitment( args.cmt_ );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  if ( !mgr.has_status( PC_PYTH_HAS_MAPPING ) ) {
    std::cerr << "pyth: mapping not ready, check mapping key ["
              << mgr.get_mapping_pub_key_file() << "]" << std::endl;
    return 1;
  }
  // list key/symbol pairs
  if ( !args.do_json_ ) {
    std::string astr;
    std::cout << "account,symbol" << std::endl;
    for(unsigned i=0; i != mgr.get_num_product(); ++i ) {
      product *prod = mgr.get_product(i);
      pub_key *akey = prod->get_account();
      akey->enc_base58( astr );
      std::cout << astr << ',' << prod->get_symbol().as_string()<< std::endl;
    }
  } else {
    json_wtr wtr;
    wtr.add_val( json_wtr::e_arr );
    for(unsigned i=0; i != mgr.get_num_product(); ++i ) {
      product *prod = mgr.get_product(i);
      pub_key *akey = prod->get_account();
      wtr.add_val( json_wtr::e_obj );
      wtr.add_key( "account", *akey );
      wtr.add_key( "symbol", prod->get_symbol() );
      wtr.pop();
    }
    wtr.pop();
    print_json( wtr );
  }

  return 0;
}

static void print_product( product *prod )
{
  std::string pkey;
  std::cout << "product details:" << std::endl;
  print_val( "product account", 2 );
  pub_key pacc( *prod->get_account() );
  pacc.enc_base58( pkey );
  std::cout << pkey << std::endl;
  print_val( "num_price", 2 );
  std::cout << prod->get_num_price() << std::endl;
  str vstr, kstr;
  for( unsigned id=1, i=0; i != prod->get_num_attr(); ) {
    attr_id aid( id++ );
    if ( !prod->get_attr( aid, vstr ) ) {
      continue;
    }
    kstr = aid.get_str();
    print_val( kstr, 2 );
    std::cout << vstr.as_string() << std::endl;
    ++i;
  }
  for( unsigned i=0; i != prod->get_num_price(); ++i ) {
    std::string ikey;
    price *ptr = prod->get_price( i );
    print_val( "price account", 2 );
    pub_key pacc( *ptr->get_account() );
    pacc.enc_base58( ikey );
    std::cout << ikey << std::endl;
    print_val( "price_type", 4 );
    std::cout << price_type_to_str( ptr->get_price_type() ).as_string()
              << std::endl;
    print_val( "price_exponent", 4 );
    std::cout << ptr->get_price_exponent() << std::endl;
    print_val( "status", 4 );
    std::cout << symbol_status_to_str( ptr->get_status() ).as_string()
              << std::endl;
    print_val( "price", 4 );
    std::cout << ptr->get_price() << std::endl;
    print_val( "conf", 4 );
    std::cout << ptr->get_conf() << std::endl;
    print_val( "twap", 4 );
    std::cout << ptr->get_twap() << std::endl;
    print_val( "twac", 4 );
    std::cout << ptr->get_twac() << std::endl;
    print_val( "valid_slot", 4 );
    std::cout << ptr->get_valid_slot() << std::endl;
    print_val( "pub_slot", 4 );
    std::cout << ptr->get_pub_slot() << std::endl;
    print_val( "prev_slot", 4 );
    std::cout << ptr->get_prev_slot() << std::endl;
    print_val( "prev_price", 4 );
    std::cout << ptr->get_prev_price() << std::endl;
    print_val( "prev_conf", 4 );
    std::cout << ptr->get_prev_conf() << std::endl;
    for( unsigned j=0; j != ptr->get_num_publisher(); ++j ) {
      ptr->get_publisher( j )->enc_base58( ikey );
      print_val( "publisher", 4 );
      std::cout << ikey << std::endl;
      print_val( "status", 6 );
      std::cout << symbol_status_to_str( ptr->get_publisher_status(j) )
        .as_string() << std::endl;
      print_val( "price", 6 );
      std::cout << ptr->get_publisher_price(j) << std::endl;
      print_val( "conf", 6 );
      std::cout << ptr->get_publisher_conf(j) << std::endl;
      print_val( "slot", 6 );
      std::cout << ptr->get_publisher_slot(j) << std::endl;
    }
  }
}

static void print_product_json( product *prod )
{
  json_wtr wtr;
  wtr.add_val( json_wtr::e_obj );
  prod->dump_json( wtr );
  wtr.pop();
  print_json( wtr );
}

int on_get_product( int argc, char **argv )
{
  if ( argc < 2 ) {
    return usage();
  }
  std::string pkey( argv[1] );
  pub_key pub;
  pub.init_from_text( pkey );
  argc -= 1;
  argv += 1;
  pyth_arguments args( argc, argv );
  if ( args.invalid_ )
    return 1;

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( args.rpc_host_ );
  mgr.set_dir( args.key_dir_ );
  mgr.set_do_tx( false );
  mgr.set_commitment( args.cmt_ );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  if ( !mgr.has_status( PC_PYTH_HAS_MAPPING ) ) {
    std::cerr << "pyth: mapping not ready, check mapping key ["
              << mgr.get_mapping_pub_key_file() << "]" << std::endl;
    return 1;
  }
  // get product and serialize to stdout
  product *prod = mgr.get_product( pub );
  if ( !prod ) {
    std::cerr << "pyth: failed to find product key=" << pkey << std::endl;
    return 1;
  }
  if ( !args.do_json_ ) {
    print_product( prod );
  } else {
    print_product_json( prod );
  }

  return 0;
}

int on_get_all_products( int argc, char **argv )
{
  pyth_arguments args( argc, argv );
  if ( args.invalid_ )
    return 1;

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( args.rpc_host_ );
  mgr.set_dir( args.key_dir_ );
  mgr.set_do_tx( false );
  mgr.set_commitment( args.cmt_ );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  if ( !mgr.has_status( PC_PYTH_HAS_MAPPING ) ) {
    std::cerr << "pyth: mapping not ready, check mapping key ["
              << mgr.get_mapping_pub_key_file() << "]" << std::endl;
    return 1;
  }

  // get all products and serialize to stdout
  if ( !args.do_json_ ) {
    for (unsigned i=0; i != mgr.get_num_product(); ++i ) {
      product *prod = mgr.get_product(i);
      print_product( prod );
      std::cout << std::endl;
    }
  } else {
    std::cout << "[";
    bool first = true;
    for (unsigned i=0; i != mgr.get_num_product(); ++i ) {
      product *prod = mgr.get_product(i);
      if ( !first ) {
        std::cout << ",";
      }
      print_product_json( prod );
      first = false;
    }
    std::cout << "]";
  }

  return 0;
}

int main(int argc, char **argv)
{
  if ( argc < 2 ) {
    return usage();
  }
  --argc;
  ++argv;

  // set up signal handing
  signal( SIGPIPE, SIG_IGN );
  log::set_level( PC_LOG_ERR_LVL );

  // dispatch by command
  std::string cmd( argv[0] );
  int rc = 0;
  if ( cmd == "init_key" ) {
    rc = on_init_key( argc, argv );
  } else if ( cmd == "upd_price" ) {
    rc = on_upd_price( argc, argv );
  } else if ( cmd == "upd_price_val" ) {
    rc = on_upd_price_val( argc, argv );
  } else if ( cmd == "get_product" ) {
    rc = on_get_product( argc, argv );
  } else if ( cmd == "get_product_list" ) {
    rc = on_get_product_list( argc, argv );
  } else if ( cmd == "get_all_products" ) {
    rc = on_get_all_products( argc, argv );
  } else if ( cmd == "version" ) {
    std::cout << "version: " << PC_VERSION << std::endl;
  } else {
    std::cerr << "pyth: unknown command" << std::endl;
    rc = usage();
  }
  return rc;
}
