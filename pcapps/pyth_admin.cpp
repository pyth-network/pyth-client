#include "admin_request.hpp"
#include <pc/manager.hpp>
#include <pc/log.hpp>
#include <pc/mem_map.hpp>
#include <unistd.h>
#include <signal.h>
#include <iostream>

// pyth_admin command-line tool

using namespace pc;

static const int         DEFAULT_EXPONENT  = -5;
static const std::string DEFAULT_RPC_HOST  = "localhost";
static const std::string DEFAULT_KEY_STORE = std::string( getenv( "HOME" ) ) + "/.pythd/";

int usage()
{
  using namespace std;
  cerr << "usage: pyth_admin" << endl;
  cerr << "  init_program     [options]" << endl;
  cerr << "  init_mapping     [options]" << endl;
  cerr << "  init_price       <price_key> "
       << "[-e <price_exponent (default " << DEFAULT_EXPONENT
       << ")>] [options]" << endl;
  cerr << "  init_test        [options]" << endl;
  cerr << "  add_product      [options]" << endl;
  cerr << "  add_price        <product_key> <price_type> "
       << "[-e <price_exponent (default " << DEFAULT_EXPONENT
       << ")>] [options]" << endl;
  cerr << "  add_publisher    <pub_key> <price_key> [options]" << endl;
  cerr << "  del_publisher    <pub_key> <price_key> [options]" << endl;
  cerr << "  upd_product      <product.json> [options]" << endl;
  cerr << "  upd_test         <test_key> <test.json> [options]" << endl;
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
  cerr << "  -d" << endl;
  cerr << "     Turn on debug logging\n" << endl;
  cerr << "  -h" << endl;
  cerr << "     Output this help text\n" << endl;
  return 1;
}

struct pyth_arguments
{
  pyth_arguments( int argc, char **argv );

  bool         invalid_    = false;
  std::string  rpc_host_   = DEFAULT_RPC_HOST;
  std::string  key_dir_    = DEFAULT_KEY_STORE;
  commitment   cmt_        = commitment::e_confirmed;
  int          exponent_   = DEFAULT_EXPONENT;
  bool         do_prompt_  = true;
};

pyth_arguments::pyth_arguments( int argc, char **argv )
{
  int opt = 0;
  while ( (opt = ::getopt( argc, argv, "r:k:c:de:nh" )) != -1 ) {
    switch (opt) {
      case 'r': rpc_host_ = optarg; break;
      case 'k': key_dir_ = optarg; break;
      case 'c': cmt_ = str_to_commitment( optarg ); break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      case 'e': exponent_ = ::atoi( optarg ); break;
      case 'n': do_prompt_ = false; break;
      default:
        usage();
        invalid_ = true;
    }
  }

  if ( cmt_ == commitment::e_unknown ) {
    std::cerr << "pyth_admin: unknown commitment level" << std::endl;
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
  std::cout.write( spaces, static_cast< std::streamsize>( sp ) );
  std::cout.write( val.str_, static_cast< std::streamsize>( val.len_ ) );
  if ( num > val.len_ ) {
    std::cout.write( dots, static_cast< std::streamsize>( num - val.len_ ) );
  }
  std::cout << ' ';
}

int on_init_program( int argc, char **argv )
{
  pyth_arguments args( argc, argv );
  if ( args.invalid_ )
    return 1;

  key_store kst;
  kst.set_dir( args.key_dir_ );
  if ( !kst.init() ) {
    std::cerr << "pyth_admin: " << kst.get_err_msg() << std::endl;
    return 1;
  }
  if ( kst.get_program_key_pair() ) {
    std::cerr << "pyth_admin: program key pair already exists ["
      << kst.get_program_key_pair_file() << "]" << std::endl;
    return 1;
  }
  if ( !kst.create_program_key_pair() ) {
    std::cerr << "pyth_admin: failed to create program key pair ["
      << kst.get_program_key_pair_file() << "]" << std::endl;
    std::cerr << "pyth_admin: " << kst.get_err_msg() << std::endl;
    return 1;
  }

  return 0;
}

int on_init_mapping( int argc, char **argv )
{
  pyth_arguments args( argc, argv );
  if ( args.invalid_ )
    return 1;

  std::cerr << "this might take take up to 30 seconds..." << std::endl;

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( args.rpc_host_ );
  mgr.set_dir( args.key_dir_ );
  mgr.set_do_tx( false );
  mgr.set_commitment( args.cmt_ );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth_admin: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // get rent-exemption amount
  get_minimum_balance_rent_exemption req_r[1];
  req_r->set_size( sizeof( pc_map_table_t ) );
  if( !mgr.submit_poll( req_r ) ) {
    return 1;
  }

  // submit init_mapping request
  init_mapping req_i[1];
  req_i->set_lamports( req_r->get_lamports() );
  req_i->set_commitment( args.cmt_ );
  if( !mgr.submit_poll( req_i ) ) {
    return 1;
  }

  return 0;
}

int on_add_product( int argc, char **argv )
{
  pyth_arguments args( argc, argv );
  if ( args.invalid_ )
    return 1;

  std::cerr << "this might take take up to 30 seconds..." << std::endl;

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( args.rpc_host_ );
  mgr.set_dir( args.key_dir_ );
  mgr.set_do_tx( false );
  mgr.set_commitment( args.cmt_ );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth_admin: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // check if we need to create a new mapping account first
  get_mapping *mptr = mgr.get_last_mapping();
  if ( mptr && mptr->get_is_full() ) {
    // get rent-exemption amount for mapping account
    get_minimum_balance_rent_exemption req_r[1];
    req_r->set_size( sizeof( pc_map_table_t ) );
    if( !mgr.submit_poll( req_r ) ) {
      return 1;
    }
    // add mapping account
    add_mapping req_m[1];
    req_m->set_lamports( req_r->get_lamports() );
    req_m->set_commitment( commitment::e_finalized );
    if( !mgr.submit_poll( req_m ) ) {
      return 1;
    }
  }

  // get rent-exemption amount for symbol account
  get_minimum_balance_rent_exemption req_r[1];
  req_r->set_size( PC_PROD_ACC_SIZE );
  if( !mgr.submit_poll( req_r ) ) {
    return 1;
  }
  // add new product account
  add_product req_a[1];
  req_a->set_lamports( req_r->get_lamports() );
  req_a->set_commitment( args.cmt_ );
  if( !mgr.submit_poll( req_a ) ) {
    return 1;
  }
  // print new key
  std::string pkstr;
  pub_key pk( *req_a->get_account() );
  pk.enc_base58( pkstr );
  std::cout << pkstr << std::endl;

  return 0;
}

int on_upd_product( int argc, char **argv )
{
  // get cfg file name
  if ( argc < 2 ) {
    return usage();
  }
  jtree   pt;
  mem_map cfg;
  cfg.set_file( argv[1] );
  argc -= 1;
  argv += 1;

  pyth_arguments args( argc, argv );
  if ( args.invalid_ )
    return 1;

  // parse config file
  if ( !cfg.init() ) {
    std::cerr << "pyth_admin: failed to read file=" << cfg.get_file() << std::endl;
    return 1;
  }
  pt.parse( cfg.data(), cfg.size() );
  if ( !pt.is_valid() ) {
    std::cerr << "pyth_admin: failed to parse file=" <<cfg.get_file() << std::endl;
    return 1;
  }

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( args.rpc_host_ );
  mgr.set_dir( args.key_dir_ );
  mgr.set_do_tx( false );
  mgr.set_commitment( args.cmt_ );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth_admin: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // update each product in file
  attr_dict adict;
  pub_key   pub;
  upd_product req[1];
  req->set_commitment( args.cmt_ );
  req->set_attr_dict( &adict );
  for( uint32_t tok = pt.get_first(1); tok; tok = pt.get_next(tok) ) {
    uint32_t acc_id = pt.find_val( tok, "account" );
    uint32_t attr_d = pt.find_val( tok, "attr_dict" );
    if ( !acc_id || !adict.init_from_json( pt, attr_d ) ) {
      std::cerr << "pyth_admin: missing/invalid account/attr_dict in file="
                << cfg.get_file() << std::endl;
      return 1;
    }
    str acc = pt.get_str( acc_id );
    pub.init_from_text( acc );
    product *prod = mgr.get_product( pub );
    if ( !prod ) {
      std::cerr << "pyth_admin: failed to find product="
        << acc.as_string() << std::endl;
      return 1;
    }
    req->reset();
    req->set_product( prod );
    if( !mgr.submit_poll( req ) ) {
      return 1;
    }
  }

  return 0;
}

int on_add_price( int argc, char **argv )
{
  // get input parameters
  if ( argc < 3 ) {
    return usage();
  }
  std::string pkey( argv[1] );
  pub_key pub;
  pub.init_from_text( pkey );
  price_type ptype = str_to_price_type( argv[2] );
  if ( ptype == price_type::e_unknown ) {
    std::cerr << "pyth_admin: unknown price_type=" << argv[3] << std::endl;
    return usage();
  }
  argc -= 2;
  argv += 2;
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
    std::cerr << "pyth_admin: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // get product
  product *prod = mgr.get_product( pub );
  if ( !prod ) {
    std::cerr << "pyth_admin: failed to find product=" << pkey << std::endl;
    return 1;
  }

  // are you sure prompt
  if ( args.do_prompt_ ) {
    std::cout << "adding new price account:" << std::endl;
    print_val( "product account", 2 );
    pub_key pacc( *prod->get_account() );
    pacc.enc_base58( pkey );
    std::cout << pkey << std::endl;
    print_val( "symbol", 2 );
    std::cout << prod->get_symbol().as_string() << std::endl;
    print_val( "price_type", 2 );
    std::cout << price_type_to_str( ptype ).as_string() << std::endl;
    print_val( "version", 2 );
    std::cout << PC_VERSION << std::endl;
    print_val( "exponent", 2 );
    std::cout << args.exponent_ << std::endl;
    std::cout << "are you sure? [y/n] ";
    char ch;
    std::cin >> ch;
    if ( ch != 'y' && ch != 'Y' ) {
      return 1;
    }
  }
  std::cerr << "this might take take up to 30 seconds..." << std::endl;

  // get rent-exemption amount for symbol account
  get_minimum_balance_rent_exemption req_r[1];
  req_r->set_size( sizeof( pc_price_t ) );
  if( !mgr.submit_poll( req_r ) ) {
    return 1;
  }
  // add new symbol account
  add_price req_a[1];
  req_a->set_exponent( args.exponent_ );
  req_a->set_price_type( ptype );
  req_a->set_lamports( req_r->get_lamports() );
  req_a->set_commitment( args.cmt_ );
  req_a->set_product( prod );
  if( !mgr.submit_poll( req_a ) ) {
    return 1;
  }
  // print new key
  std::string pkstr;
  pub_key pk( *req_a->get_account() );
  pk.enc_base58( pkstr );
  std::cout << pkstr << std::endl;

  return 0;
}

int on_init_price( int argc, char **argv )
{
  // get input parameters
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
    std::cerr << "pyth_admin: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // get price
  price *px = mgr.get_price( pub );
  if ( !px ) {
    std::cerr << "pyth_admin: failed to find price=" << pkey << std::endl;
    return 1;
  }

  // are you sure prompt
  if ( args.do_prompt_ ) {
    std::cout << "initialize price account:" << std::endl;
    print_val( "price account", 2 );
    pub_key pacc( *px->get_account() );
    pacc.enc_base58( pkey );
    std::cout << pkey << std::endl;
    print_val( "symbol", 2 );
    std::cout << px->get_symbol().as_string() << std::endl;
    print_val( "price_type", 2 );
    std::cout << price_type_to_str( px->get_price_type() ).as_string()
              << std::endl;
    print_val( "version", 2 );
    std::cout << PC_VERSION << std::endl;
    print_val( "exponent", 2 );
    std::cout << args.exponent_ << std::endl;
    std::cout << "are you sure? [y/n] ";
    char ch;
    std::cin >> ch;
    if ( ch != 'y' && ch != 'Y' ) {
      return 1;
    }
  }

  // add new symbol account
  init_price req_i[1];
  req_i->set_exponent( args.exponent_ );
  req_i->set_commitment( args.cmt_ );
  req_i->set_price( px );
  if( !mgr.submit_poll( req_i ) ) {
    return 1;
  }

  return 0;
}

int on_init_test( int argc, char **argv )
{
  pyth_arguments args( argc, argv );
  if ( args.invalid_ )
    return 1;

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( args.rpc_host_ );
  mgr.set_dir( args.key_dir_ );
  mgr.set_do_tx( false );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth_admin: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  // get rent-exemption amount
  get_minimum_balance_rent_exemption req_r[1];
  req_r->set_size( sizeof( pc_price_t ) );
  if( !mgr.submit_poll( req_r ) ) {
    return 1;
  }
  std::cerr << "this might take up to 30 seconds..." << std::endl;

  // submit init_test request
  init_test req_i[1];
  req_i->set_lamports( req_r->get_lamports() );
  if( !mgr.submit_poll( req_i ) ) {
    return 1;
  }

  // print new key
  std::string pkstr;
  pub_key pk( *req_i->get_account() );
  pk.enc_base58( pkstr );
  std::cout << pkstr << std::endl;

  return 0;
}

int on_upd_test( int argc, char **argv )
{
  // get input parameters
  if ( argc < 3 ) {
    return usage();
  }
  std::string test_key = argv[1];
  std::string test_file = argv[2];
  argc -= 2;
  argv += 2;
  pyth_arguments args( argc, argv );
  if ( args.invalid_ )
    return 1;

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( args.rpc_host_ );
  mgr.set_dir( args.key_dir_ );
  mgr.set_do_tx( false );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth_admin: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // initialize and submit upd_test request
  upd_test req_u[1];
  req_u->set_test_key( test_key );
  if ( !req_u->init_from_file( test_file ) ) {
    std::cerr << "pyth_admin: " << req_u->get_err_msg() << std::endl;
    return 1;
  }
  if( !mgr.submit_poll( req_u ) ) {
    return 1;
  }

  return 0;
}

int on_upd_publisher( int argc, char **argv, bool is_add )
{
  // get input parameters
  if ( argc < 3 ) {
    return usage();
  }
  std::string pub_str( argv[1] ), prc_str( argv[2] );
  pub_key pub_key, prc_key;
  pub_key.init_from_text( pub_str );
  prc_key.init_from_text( prc_str );
  argc -= 2;
  argv += 2;
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
    std::cerr << "pyth_admin: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  price *ptr = mgr.get_price( prc_key );
  if ( !ptr ) {
    std::cerr << "pyth_admin: failed to find price=" << prc_str << std::endl;
    return 1;
  }

  // are you sure prompt
  if ( args.do_prompt_ ) {
    std::cout << (is_add?"adding new":"delete")
              << " price publisher:" << std::endl;
    print_val( "publisher account", 2 );
    std::cout << pub_str << std::endl;
    print_val( "price account", 2 );
    std::cout << prc_str << std::endl;
    print_val( "symbol", 2 );
    std::cout << ptr->get_symbol().as_string() << std::endl;
    print_val( "price_type", 2 );
    std::cout << price_type_to_str( ptr->get_price_type() ).as_string()
              << std::endl;
    print_val( "price_exponent", 2 );
    std::cout << ptr->get_price_exponent() << std::endl;
    std::cout << "are you sure? [y/n] ";
    char ch;
    std::cin >> ch;
    if ( ch != 'y' && ch != 'Y' ) {
      return 1;
    }
  }

  // submit request
  if ( is_add ) {
    add_publisher req[1];
    req->set_price( ptr );
    req->set_publisher( pub_key );
    req->set_commitment( args.cmt_ );
    return !mgr.submit_poll( req );
  } else {
    del_publisher req[1];
    req->set_price( ptr );
    req->set_publisher( pub_key );
    req->set_commitment( args.cmt_ );
    return !mgr.submit_poll( req );
  }
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
  if ( cmd == "init_program" ) {
    rc = on_init_program( argc, argv );
  } else if ( cmd == "init_mapping" ) {
    rc = on_init_mapping( argc, argv );
  } else if ( cmd == "init_price" ) {
    rc = on_init_price( argc, argv );
  } else if ( cmd == "init_test" ) {
    rc = on_init_test( argc, argv );
  } else if ( cmd == "add_product" ) {
    rc = on_add_product( argc, argv );
  } else if ( cmd == "add_price" ) {
    rc = on_add_price( argc, argv );
  } else if ( cmd == "add_publisher" ) {
    rc = on_upd_publisher( argc, argv, true );
  } else if ( cmd == "del_publisher" ) {
    rc = on_upd_publisher( argc, argv, false );
  } else if ( cmd == "upd_product" ) {
    rc = on_upd_product( argc, argv );
  } else if ( cmd == "upd_test" ) {
    rc = on_upd_test( argc, argv );
  } else if ( cmd == "version" ) {
    std::cout << "version: " << PC_VERSION << std::endl;
  } else {
    std::cerr << "pyth_admin: unknown command" << std::endl;
    rc = usage();
  }
  return rc;
}
