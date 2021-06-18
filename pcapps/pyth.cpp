#include <pc/manager.hpp>
#include <pc/log.hpp>
#include <pc/misc.hpp>
#include <pc/mem_map.hpp>
#include <unistd.h>
#include <signal.h>
#include <iostream>

// pyth command-line tool

int get_exponent()
{
  return -5;
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

using namespace pc;

int usage()
{
  std::cerr << "usage: pyth " << std::endl;
  std::cerr << "  init_key         [options]" << std::endl;
  std::cerr << "  init_program     [options]" << std::endl;
  std::cerr << "  init_mapping     [options]" << std::endl;
  std::cerr << "  init_price       <price_key> "
            << "[-e <price_exponent (default " << get_exponent()
            << ")>] [options]" << std::endl;
  std::cerr << "  init_test        [options]" << std::endl;
  std::cerr << "  transfer         <pub_key> <amount> [options]"
            << std::endl;
  std::cerr << "  add_product      [options]" << std::endl;
  std::cerr << "  add_price        <prod_key> <price_type> "
            << "[-e <price_exponent (default " << get_exponent()
            << ")>] [options]" << std::endl;
  std::cerr << "  add_publisher    <pub_key> <price_key> [options]"
            << std::endl;
  std::cerr << "  del_publisher    <pub_key> <price_key> [options]"
            << std::endl;
  std::cerr << "  upd_product      <product.json> [options]" << std::endl;
  std::cerr << "  upd_price        <price_key> [options]"
            << std::endl;
  std::cerr << "  upd_test         <test_key> <test.json> [options]"
            << std::endl;
  std::cerr << "  get_balance      [<pub_key>] [options]" << std::endl;
  std::cerr << "  get_block        <slot_number> [options]" << std::endl;
  std::cerr << "  get_product      <prod_key> [options]" << std::endl;
  std::cerr << "  get_product_list [options]" << std::endl;
  std::cerr << "  get_pub_key      <key_pair_file>" << std::endl;
  std::cerr << "  version" << std::endl;
  std::cerr << std::endl;

  std::cerr << "options include:" << std::endl;
  std::cerr << "  -r <rpc_host (default " << get_rpc_host() << ")>"
             << std::endl;
  std::cerr << "     Host name or IP address of solana rpc node in the form "
               "host_name[:rpc_port[:ws_port]]\n" << std::endl;
  std::cerr << "  -k <key_store_directory (default "
            << get_key_store() << ">" << std::endl;
  std::cerr << "     Directory name housing publishing, mapping and program"
               " key files\n" << std::endl;
  std::cerr << "  -c <commitment_level (default confirmed)>" << std::endl;
  std::cerr << "     Options include processed, confirmed and finalized\n"
            << std::endl;
  std::cerr << "  -j\n"
            << "     Output results in json format where applicable\n"
            << std::endl;
  return 1;
}

static const char spaces[] =
"..........................................................................";

static const char dots[] =
"                                                                          ";

void print( str val, size_t sp=0 )
{
  size_t num = 20 - sp;
  std::cout.write( dots, sp );
  std::cout.write( val.str_, val.len_ );
  if ( num > val.len_ ) {
    std::cout.write( spaces, num - val.len_ );
  }
  std::cout << ' ';
}

int submit_request( manager& mgr, request *req )
{
  // submit request and poll for completion or error
  mgr.submit( req );
  while( !req->get_is_done() &&
         !req->get_is_err() &&
         !mgr.get_is_err() ) {
    mgr.poll();
  }
  if ( req->get_is_err() ) {
    std::cerr << "pyth: " << req->get_err_msg() << std::endl;
    return 1;
  }
  if ( mgr.get_is_err() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  return 0;
}

int on_init_key( int argc, char **argv )
{
  int opt = 0;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "r:k:c:dh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      case 'c': break;
      default: return usage();
    }
  }
  key_store kst;
  kst.set_dir( key_dir );
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

int on_init_program( int argc, char **argv )
{
  int opt = 0;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "r:k:c:dh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      case 'c': break;
      default: return usage();
    }
  }
  key_store kst;
  kst.set_dir( key_dir );
  if ( !kst.init() ) {
    std::cerr << "pyth: " << kst.get_err_msg() << std::endl;
    return 1;
  }
  if ( kst.get_program_key_pair() ) {
    std::cerr << "pyth: program key pair already exists ["
      << kst.get_program_key_pair_file() << "]" << std::endl;
    return 1;
  }
  if ( !kst.create_program_key_pair() ) {
    std::cerr << "pyth: failed to create program key pair ["
      << kst.get_program_key_pair_file() << "]" << std::endl;
    std::cerr << "pyth: " << kst.get_err_msg() << std::endl;
    return 1;
  }

  return 0;
}

int on_init_mapping( int argc, char **argv )
{
  // get input parameters
  int opt = 0;
  commitment cmt = commitment::e_confirmed;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "r:k:c:dh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      case 'c': cmt = str_to_commitment(optarg); break;
      default: return usage();
    }
  }
  if ( cmt == commitment::e_unknown ) {
    std::cerr << "pyth: unknown commitment level" << std::endl;
    return usage();
  }
  std::cerr << "this might take take up to 30 seconds..." << std::endl;

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  mgr.set_do_tx( false );
  mgr.set_commitment( cmt );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // get rent-exemption amount
  get_minimum_balance_rent_exemption req_r[1];
  req_r->set_size( sizeof( pc_map_table_t ) );
  if( submit_request( mgr, req_r ) ) {
    return 1;
  }

  // submit init_mapping request
  init_mapping req_i[1];
  req_i->set_lamports( req_r->get_lamports() );
  req_i->set_commitment( cmt );
  if( submit_request( mgr, req_i ) ) {
    return 1;
  }
  return 0;
}

int on_transfer( int argc, char **argv )
{
  // get input parameters
  if ( argc < 3 ) {
    return usage();
  }
  pub_key pub;
  pub.init_from_text( str( argv[1] ) );
  int64_t lamports = str_to_dec( argv[2], -9 );
  if ( lamports <= 0 ) {
    return usage();
  }
  argc -= 2;
  argv += 2;
  int opt = 0;
  commitment cmt = commitment::e_confirmed;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "r:k:c:dh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      case 'c': cmt = str_to_commitment(optarg); break;
      default: return usage();
    }
  }
  if ( cmt == commitment::e_unknown ) {
    std::cerr << "pyth: unknown commitment level" << std::endl;
    return usage();
  }
  std::string knm;
  pub.enc_base58( knm );
  std::cout << "transfer to account:" << std::endl;
  print( "account", 2 );
  std::cout << knm << std::endl;
  print( "amount", 2 );
  printf( "%.9f\n", 1e-9*lamports );
  std::cout << "are you sure? [y/n] ";
  char ch;
  std::cin >> ch;
  if ( ch != 'y' && ch != 'Y' ) {
    return 1;
  }
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  mgr.set_do_tx( false );
  mgr.set_commitment( cmt );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // submit request
  transfer req[1];
  req->set_lamports( lamports );
  req->set_commitment( cmt );
  req->set_receiver( &pub );
  return submit_request( mgr, req );
}

int on_get_balance( int argc, char **argv )
{
  pub_key pub;
  std::string knm;
  if ( argc > 1 && argv[1][0] != '-' ) {
    knm = argv[1];
  }
  int opt = 0;
  commitment cmt = commitment::e_confirmed;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "r:k:p:c:h" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'p': knm = optarg; break;
      case 'c': cmt = str_to_commitment(optarg); break;
      default: return usage();
    }
  }
  if ( cmt == commitment::e_unknown ) {
    std::cerr << "pyth: unknown commitment level" << std::endl;
    return usage();
  }

  if ( knm.empty() ) {
    key_store mgr;
    mgr.set_dir( key_dir );
    if ( !mgr.init() ) {
      std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
      return 1;
    }
    pub = *mgr.get_publish_pub_key();
  } else {
    pub.init_from_text( knm );
  }
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  mgr.set_do_tx( false );
  mgr.set_commitment( cmt );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  balance req[1];
  req->set_pub_key( &pub );
  int ret = submit_request( mgr, req );
  if ( ret == 0 ) {
    std::cout << "balance details:" << std::endl;
    pub.enc_base58( knm );
    print( "account", 2 );
    std::cout << knm << std::endl;
    print( "balance", 2 );
    printf( "%.9f\n", 1e-9*req->get_lamports() );
  }
  return ret;
}

int on_add_product( int argc, char **argv )
{
  // get input parameters
  int opt = 0;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  commitment cmt = commitment::e_confirmed;
  while( (opt = ::getopt(argc,argv, "e:r:k:c:dh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'c': cmt = str_to_commitment(optarg); break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      default: return usage();
    }
  }
  if ( cmt == commitment::e_unknown ) {
    std::cerr << "pyth: unknown commitment level" << std::endl;
    return usage();
  }
  std::cerr << "this might take take up to 30 seconds..." << std::endl;

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  mgr.set_do_tx( false );
  mgr.set_commitment( cmt );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // check if we need to create a new mapping account first
  get_mapping *mptr = mgr.get_last_mapping();
  if ( mptr && mptr->get_is_full() ) {
    // get rent-exemption amount for mapping account
    get_minimum_balance_rent_exemption req_r[1];
    req_r->set_size( sizeof( pc_map_table_t ) );
    if( submit_request( mgr, req_r ) ) {
      return 1;
    }
    // add mapping account
    add_mapping req_m[1];
    req_m->set_lamports( req_r->get_lamports() );
    req_m->set_commitment( commitment::e_finalized );
    if( submit_request( mgr, req_m ) ) {
      return 1;
    }
  }

  // get rent-exemption amount for symbol account
  get_minimum_balance_rent_exemption req_r[1];
  req_r->set_size( PC_PROD_ACC_SIZE );
  if( submit_request( mgr, req_r ) ) {
    return 1;
  }
  // add new product account
  add_product req_a[1];
  req_a->set_lamports( req_r->get_lamports() );
  req_a->set_commitment( cmt );
  if( submit_request( mgr, req_a ) ) {
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

  // get other options
  int opt = 0;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  commitment cmt = commitment::e_confirmed;
  while( (opt = ::getopt(argc,argv, "r:k:c:dh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'c': cmt = str_to_commitment(optarg); break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      default: return usage();
    }
  }

  // parse config file
  if ( !cfg.init() ) {
    std::cerr << "pyth: failed to read file=" << cfg.get_file() << std::endl;
    return 1;
  }
  pt.parse( cfg.data(), cfg.size() );
  if ( !pt.is_valid() ) {
    std::cerr << "pyth: failed to parse file=" <<cfg.get_file() << std::endl;
    return 1;
  }

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  mgr.set_do_tx( false );
  mgr.set_commitment( cmt );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // update each product in file
  attr_dict adict;
  pub_key   pub;
  upd_product req[1];
  req->set_commitment( cmt );
  req->set_attr_dict( &adict );
  for( uint32_t tok = pt.get_first(1); tok; tok = pt.get_next(tok) ) {
    uint32_t acc_id = pt.find_val( tok, "account" );
    uint32_t attr_d = pt.find_val( tok, "attr_dict" );
    if ( !acc_id || !adict.init_from_json( pt, attr_d ) ) {
      std::cerr << "pyth: missing/invalid account/attr_dict in file="
                << cfg.get_file() << std::endl;
      return 1;
    }
    str acc = pt.get_str( acc_id );
    pub.init_from_text( acc );
    product *prod = mgr.get_product( pub );
    if ( !prod ) {
      std::cerr << "pyth: failed to find product="
        << acc.as_string() << std::endl;
      return 1;
    }
    req->reset();
    req->set_product( prod );
    if( submit_request( mgr, req ) ) {
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
    std::cerr << "pyth: unknown price_type=" << argv[3] << std::endl;
    return usage();
  }
  argc -= 2;
  argv += 2;
  bool do_prompt = true;
  int opt = 0, exponent = get_exponent();
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  commitment cmt = commitment::e_confirmed;
  while( (opt = ::getopt(argc,argv, "e:r:k:c:ndh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'c': cmt = str_to_commitment(optarg); break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      case 'e': exponent = ::atoi( optarg ); break;
      case 'n': do_prompt = false; break;
      default: return usage();
    }
  }
  if ( cmt == commitment::e_unknown ) {
    std::cerr << "pyth: unknown commitment level" << std::endl;
    return usage();
  }

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  mgr.set_do_tx( false );
  mgr.set_commitment( cmt );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // get product
  product *prod = mgr.get_product( pub );
  if ( !prod ) {
    std::cerr << "pyth: failed to find product=" << pkey << std::endl;
    return 1;
  }

  // are you sure prompt
  if ( do_prompt ) {
    std::cout << "adding new price account:" << std::endl;
    print( "product account", 2 );
    pub_key pacc( *prod->get_account() );
    pacc.enc_base58( pkey );
    std::cout << pkey << std::endl;
    print( "symbol", 2 );
    std::cout << prod->get_symbol().as_string() << std::endl;
    print( "price_type", 2 );
    std::cout << price_type_to_str( ptype ).as_string() << std::endl;
    print( "version", 2 );
    std::cout << PC_VERSION << std::endl;
    print( "exponent", 2 );
    std::cout << exponent << std::endl;
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
  if( submit_request( mgr, req_r ) ) {
    return 1;
  }
  // add new symbol account
  add_price req_a[1];
  req_a->set_exponent( exponent );
  req_a->set_price_type( ptype );
  req_a->set_lamports( req_r->get_lamports() );
  req_a->set_commitment( cmt );
  req_a->set_product( prod );
  if( submit_request( mgr, req_a ) ) {
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
  bool do_prompt = true;
  int opt = 0, exponent = get_exponent();
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  commitment cmt = commitment::e_confirmed;
  while( (opt = ::getopt(argc,argv, "e:r:k:c:ndh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'c': cmt = str_to_commitment(optarg); break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      case 'e': exponent = ::atoi( optarg ); break;
      case 'n': do_prompt = false; break;
      default: return usage();
    }
  }
  if ( cmt == commitment::e_unknown ) {
    std::cerr << "pyth: unknown commitment level" << std::endl;
    return usage();
  }

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  mgr.set_do_tx( false );
  mgr.set_commitment( cmt );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // get price
  price *px = mgr.get_price( pub );
  if ( !px ) {
    std::cerr << "pyth: failed to find price=" << pkey << std::endl;
    return 1;
  }

  // are you sure prompt
  if ( do_prompt ) {
    std::cout << "initialize price account:" << std::endl;
    print( "price account", 2 );
    pub_key pacc( *px->get_account() );
    pacc.enc_base58( pkey );
    std::cout << pkey << std::endl;
    print( "symbol", 2 );
    std::cout << px->get_symbol().as_string() << std::endl;
    print( "price_type", 2 );
    std::cout << price_type_to_str( px->get_price_type() ).as_string()
              << std::endl;
    print( "version", 2 );
    std::cout << PC_VERSION << std::endl;
    print( "exponent", 2 );
    std::cout << exponent << std::endl;
    std::cout << "are you sure? [y/n] ";
    char ch;
    std::cin >> ch;
    if ( ch != 'y' && ch != 'Y' ) {
      return 1;
    }
  }

  // add new symbol account
  init_price req_i[1];
  req_i->set_exponent( exponent );
  req_i->set_commitment( cmt );
  req_i->set_price( px );
  if( submit_request( mgr, req_i ) ) {
    return 1;
  }
  return 0;
}

int on_init_test( int argc, char **argv )
{
  // get input parameters
  int opt = 0;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "e:r:k:c:dh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'c': break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      default: return usage();
    }
  }
  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  mgr.set_do_tx( false );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  // get rent-exemption amount
  get_minimum_balance_rent_exemption req_r[1];
  req_r->set_size( sizeof( pc_price_t ) );
  if( submit_request( mgr, req_r ) ) {
    return 1;
  }
  std::cerr << "this might take up to 30 seconds..." << std::endl;

  // submit init_test request
  init_test req_i[1];
  req_i->set_lamports( req_r->get_lamports() );
  if( submit_request( mgr, req_i ) ) {
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
  int opt = 0;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "e:r:k:c:dh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'c': break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      default: return usage();
    }
  }
  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  mgr.set_do_tx( false );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // initialize and submit upd_test request
  upd_test req_u[1];
  req_u->set_test_key( test_key );
  if ( !req_u->init_from_file( test_file ) ) {
    std::cerr << "pyth: " << req_u->get_err_msg() << std::endl;
    return 1;
  }
  if( submit_request( mgr, req_u ) ) {
    return 1;
  }
  return 0;
}

int on_upd_price( int argc, char **argv )
{
  if ( argc < 2 ) {
    return usage();
  }
  std::string pkey( argv[1] );
  pub_key pub;
  pub.init_from_text( pkey );
  argc -= 1;
  argv += 1;

  int opt = 0;
  commitment cmt = commitment::e_confirmed;
  std::string rpc_host = get_rpc_host();
  std::string tx_host  = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "r:t:k:c:dh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 't': tx_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      case 'c': cmt = str_to_commitment(optarg); break;
      default: return usage();
    }
  }
  if ( cmt == commitment::e_unknown ) {
    std::cerr << "pyth: unknown commitment level" << std::endl;
    return usage();
  }

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_tx_host( tx_host );
  mgr.set_dir( key_dir );
  mgr.set_do_tx( true );
  mgr.set_commitment( cmt );
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
  while( mgr.get_is_tx_send() && !mgr.get_is_err() ) {
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
  int opt = 0;
  bool do_prompt = true;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  commitment cmt = commitment::e_confirmed;
  while( (opt = ::getopt(argc,argv, "e:r:k:c:ndh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      case 'c': cmt = str_to_commitment(optarg); break;
      case 'n': do_prompt = false; break;
      default: return usage();
    }
  }
  if ( cmt == commitment::e_unknown ) {
    std::cerr << "pyth: unknown commitment level" << std::endl;
    return usage();
  }

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  mgr.set_do_tx( false );
  mgr.set_commitment( cmt );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  price *ptr = mgr.get_price( prc_key );
  if ( !ptr ) {
    std::cerr << "pyth: failed to find price=" << prc_str << std::endl;
    return 1;
  }

  // are you sure prompt
  if ( do_prompt ) {
    std::cout << (is_add?"adding new":"delete")
              << " price publisher:" << std::endl;
    print( "publisher account", 2 );
    std::cout << pub_str << std::endl;
    print( "price account", 2 );
    std::cout << prc_str << std::endl;
    print( "symbol", 2 );
    std::cout << ptr->get_symbol().as_string() << std::endl;
    print( "price_type", 2 );
    std::cout << price_type_to_str( ptr->get_price_type() ).as_string()
              << std::endl;
    print( "price_exponent", 2 );
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
    req->set_commitment( cmt );
    return submit_request( mgr, req );
  } else {
    del_publisher req[1];
    req->set_price( ptr );
    req->set_publisher( pub_key );
    req->set_commitment( cmt );
    return submit_request( mgr, req );
  }
}

int on_get_pub_key( int argc, char **argv )
{
  if ( argc < 2 ) {
    return usage();
  }
  key_pair kp;
  if ( !kp.init_from_file( argv[1] ) ) {
    std::cerr << "pyth: failed to init key_pair from " << argv[1]
              << std::endl;
    return 1;
  }
  pub_key pk( kp );
  std::string pnm;
  pk.enc_base58( pnm );
  std::cout << pnm << std::endl;
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
  int opt = 0;
  bool do_json = false;
  commitment cmt = commitment::e_confirmed;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "r:k:c:djh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      case 'j': do_json = true; break;
      case 'c': cmt = str_to_commitment(optarg); break;
      default: return usage();
    }
  }
  if ( cmt == commitment::e_unknown ) {
    std::cerr << "pyth: unknown commitment level" << std::endl;
    return usage();
  }

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  mgr.set_do_tx( false );
  mgr.set_commitment( cmt );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  // list key/symbol pairs
  if ( !do_json ) {
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
  print( "product account", 2 );
  pub_key pacc( *prod->get_account() );
  pacc.enc_base58( pkey );
  std::cout << pkey << std::endl;
  print( "num_price", 2 );
  std::cout << prod->get_num_price() << std::endl;
  str vstr, kstr;
  for( unsigned id=1, i=0; i != prod->get_num_attr(); ) {
    attr_id aid( id++ );
    if ( !prod->get_attr( aid, vstr ) ) {
      continue;
    }
    kstr = aid.get_str();
    print( kstr, 2 );
    std::cout << vstr.as_string() << std::endl;
    ++i;
  }
  for( unsigned i=0; i != prod->get_num_price(); ++i ) {
    std::string ikey;
    price *ptr = prod->get_price( i );
    print( "price account", 2 );
    pub_key pacc( *ptr->get_account() );
    pacc.enc_base58( ikey );
    std::cout << ikey << std::endl;
    print( "price_type", 4 );
    std::cout << price_type_to_str( ptr->get_price_type() ).as_string()
              << std::endl;
    print( "price_exponent", 4 );
    std::cout << ptr->get_price_exponent() << std::endl;
    print( "status", 4 );
    std::cout << symbol_status_to_str( ptr->get_status() ).as_string()
              << std::endl;
    print( "price", 4 );
    std::cout << ptr->get_price() << std::endl;
    print( "conf", 4 );
    std::cout << ptr->get_conf() << std::endl;
    print( "twap", 4 );
    std::cout << ptr->get_twap() << std::endl;
    print( "twac", 4 );
    std::cout << ptr->get_twac() << std::endl;
    print( "valid_slot", 4 );
    std::cout << ptr->get_valid_slot() << std::endl;
    print( "pub_slot", 4 );
    std::cout << ptr->get_pub_slot() << std::endl;
    print( "prev_slot", 4 );
    std::cout << ptr->get_prev_slot() << std::endl;
    print( "prev_price", 4 );
    std::cout << ptr->get_prev_price() << std::endl;
    print( "prev_conf", 4 );
    std::cout << ptr->get_prev_conf() << std::endl;
    for( unsigned j=0; j != ptr->get_num_publisher(); ++j ) {
      ptr->get_publisher( j )->enc_base58( ikey );
      print( "publisher", 4 );
      std::cout << ikey << std::endl;
      print( "status", 6 );
      std::cout << symbol_status_to_str( ptr->get_publisher_status(j) )
        .as_string() << std::endl;
      print( "price", 6 );
      std::cout << ptr->get_publisher_price(j) << std::endl;
      print( "conf", 6 );
      std::cout << ptr->get_publisher_conf(j) << std::endl;
      print( "slot", 6 );
      std::cout << ptr->get_publisher_slot(j) << std::endl;
    }
  }
}

static void print_product_json( product *prod )
{
  json_wtr wtr;
  wtr.add_val( json_wtr::e_obj );
  wtr.add_key( "account", *prod->get_account() );
  wtr.add_key( "attr_dict", json_wtr::e_obj );
  str vstr, kstr;
  for( unsigned id=1, i=0; i != prod->get_num_attr(); ) {
    attr_id aid( id++ );
    if ( !prod->get_attr( aid, vstr ) ) {
      continue;
    }
    kstr = aid.get_str();
    wtr.add_key( kstr, vstr );
    ++i;
  }
  wtr.pop();
  wtr.add_key( "price_accounts", json_wtr::e_arr );
  for( unsigned i=0; i != prod->get_num_price(); ++i ) {
    wtr.add_val( json_wtr::e_obj );
    price *ptr = prod->get_price( i );
    wtr.add_key( "account", *ptr->get_account() );
    wtr.add_key( "price_type", price_type_to_str( ptr->get_price_type() ));
    wtr.add_key( "price_exponent", ptr->get_price_exponent() );
    wtr.add_key( "status", symbol_status_to_str( ptr->get_status() ) );
    wtr.add_key( "price", ptr->get_price() );
    wtr.add_key( "conf", ptr->get_conf() );
    wtr.add_key( "twap", ptr->get_twap() );
    wtr.add_key( "twac", ptr->get_twac() );
    wtr.add_key( "valid_slot", ptr->get_valid_slot() );
    wtr.add_key( "pub_slot", ptr->get_pub_slot() );
    wtr.add_key( "prev_slot", ptr->get_prev_slot() );
    wtr.add_key( "prev_price", ptr->get_prev_price() );
    wtr.add_key( "prev_conf", ptr->get_prev_conf() );
    wtr.add_key( "publisher_accounts", json_wtr::e_arr );
    for( unsigned j=0; j != ptr->get_num_publisher(); ++j ) {
      wtr.add_val( json_wtr::e_obj );
      wtr.add_key( "account", *ptr->get_publisher( j ) );
      wtr.add_key( "status", symbol_status_to_str(
            ptr->get_publisher_status(j) ) );
      wtr.add_key( "price", ptr->get_publisher_price(j) );
      wtr.add_key( "conf", ptr->get_publisher_conf(j) );
      wtr.add_key( "slot", ptr->get_publisher_slot(j) );
      wtr.pop();
    }
    wtr.pop();
    wtr.pop();
  }
  wtr.pop();
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

  int opt = 0;
  bool do_json = false;
  commitment cmt = commitment::e_confirmed;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "r:k:c:djh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      case 'j': do_json = true; break;
      case 'c': cmt = str_to_commitment(optarg); break;
      default: return usage();
    }
  }
  if ( cmt == commitment::e_unknown ) {
    std::cerr << "pyth: unknown commitment level" << std::endl;
    return usage();
  }

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  mgr.set_do_tx( false );
  mgr.set_commitment( cmt );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  // get product and serialize to stdout
  product *prod = mgr.get_product( pub );
  if ( !prod ) {
    std::cerr << "pyth: failed to find product key=" << pkey << std::endl;
    return 1;
  }
  if ( !do_json ) {
    print_product( prod );
  } else {
    print_product_json( prod );
  }
  return 0;
}

class get_block_print : public get_block
{
public:
  get_block_print( manager *mgr ) : mgr_( mgr ) {
  }
  void on_upd_price( rpc::get_block *res ) {
    std::string kstr;
    std::cout << "upd_price:" << std::endl;
    print( "publisher", 2 );
    res->get_key( 0 )->enc_base58( kstr );
    std::cout << kstr << std::endl;
    print( "price_account", 2 );
    res->get_key( 1 )->enc_base58( kstr );
    std::cout << kstr << std::endl;
    price *px = mgr_->get_price( *res->get_key(1) );
    if ( px ) {
      print( "symbol", 2 );
      std::cout << px->get_symbol().as_string() << std::endl;
    }
    cmd_upd_price *cmd = (cmd_upd_price*)res->get_cmd();
    print( "status", 2 );
    std::cout << symbol_status_to_str(
        (symbol_status)cmd->status_ ).as_string() << std::endl;
    print( "price", 2 );
    std::cout << cmd->price_ << std::endl;
    print( "conf_interval", 2 );
    std::cout << cmd->conf_ << std::endl;
    print( "pub_slot", 2 );
    std::cout << cmd->pub_slot_ << std::endl;
    std::cout << std::endl;
  }
  void on_response( rpc::get_block *res ) override {
    if ( res->get_is_err() ) {
      on_error_sub( res->get_err_msg(), this );
      st_ = e_error;
      return;
    }
    if ( res->get_is_end() ) {
      st_ = e_done;
      return;
    }
    char *cbuf = res->get_cmd();
    cmd_hdr *hdr = (cmd_hdr*)cbuf;
    switch( hdr->cmd_ ) {
      case e_cmd_upd_price: on_upd_price( res ); break;
    }
  }
  manager *mgr_;
};

class get_block_json : public get_block
{
public:
  get_block_json( manager *mgr, json_wtr& wtr ) : mgr_( mgr ), wtr_( wtr ) {
  }
  void on_upd_price( rpc::get_block *res ) {
    wtr_.add_val( json_wtr::e_obj );
    wtr_.add_key( "event", "upd_price" );
    wtr_.add_key( "publisher", *res->get_key( 0 ) );
    wtr_.add_key( "price_account", *res->get_key( 1 ) );
    wtr_.add_key( "param_account", *res->get_key( 2 ) );
    price *px = mgr_->get_price( *res->get_key(1) );
    if ( px ) {
      wtr_.add_key( "symbol", px->get_symbol() );
    }
    cmd_upd_price *cmd = (cmd_upd_price*)res->get_cmd();
    wtr_.add_key( "status", symbol_status_to_str(
        (symbol_status)cmd->status_ ).as_string() );
    wtr_.add_key( "price", cmd->price_ );
    wtr_.add_key( "conf", cmd->conf_ );
    wtr_.add_key( "pub_slot", cmd->pub_slot_ );
    wtr_.pop();
  }
  void on_response( rpc::get_block *res ) override {
    if ( res->get_is_err() ) {
      on_error_sub( res->get_err_msg(), this );
      st_ = e_error;
      return;
    }
    if ( res->get_is_end() ) {
      st_ = e_done;
      return;
    }
    char *cbuf = res->get_cmd();
    cmd_hdr *hdr = (cmd_hdr*)cbuf;
    switch( hdr->cmd_ ) {
      case e_cmd_upd_price: on_upd_price( res ); break;
    }
  }
  manager  *mgr_;
  json_wtr& wtr_;
};


int on_get_block( int argc, char **argv )
{
  if ( argc < 2 ) {
    return usage();
  }
  uint64_t slot = ::atol( argv[1] );
  if ( slot == 0 ) {
    std::cerr << "pyth: invalid slot=" << argv[1] << std::endl;
    return 1;
  }
  argc -= 1;
  argv += 1;

  int opt = 0;
  bool do_json = false;
  commitment cmt = commitment::e_finalized;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "r:k:c:djh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      case 'j': do_json = true; break;
      case 'c': cmt = str_to_commitment(optarg); break;
      default: return usage();
    }
  }
  if ( cmt == commitment::e_unknown ) {
    std::cerr << "pyth: unknown commitment level" << std::endl;
    return usage();
  }

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  mgr.set_do_tx( false );
  mgr.set_commitment( cmt );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // get transaction
  int ret = 0;
  if ( do_json ) {
    json_wtr wtr;
    wtr.add_val( json_wtr::e_arr );
    get_block_json req( &mgr, wtr );
    req.set_slot( slot );
    req.set_commitment( cmt );
    ret = submit_request( mgr, &req );
    wtr.pop();
    print_json( wtr );
  } else {
    get_block_print req( &mgr );
    req.set_slot( slot );
    req.set_commitment( cmt );
    ret = submit_request( mgr, &req );
  }
  return ret;
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
  str cmd( argv[0] );
  int rc = 0;
  if ( cmd == "init_key" ) {
    rc = on_init_key( argc, argv );
  } else if ( cmd == "init_program" ) {
    rc = on_init_program( argc, argv );
  } else if ( cmd == "init_mapping" ) {
    rc = on_init_mapping( argc, argv );
  } else if ( cmd == "init_price" ) {
    rc = on_init_price( argc, argv );
  } else if ( cmd == "init_test" ) {
    rc = on_init_test( argc, argv );
  } else if ( cmd == "transfer" ) {
    rc = on_transfer( argc, argv );
  } else if ( cmd == "get_balance" ) {
    rc = on_get_balance( argc, argv );
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
  } else if ( cmd == "upd_price" ) {
    rc = on_upd_price( argc, argv );
  } else if ( cmd == "upd_test" ) {
    rc = on_upd_test( argc, argv );
  } else if ( cmd == "get_pub_key" ) {
    rc = on_get_pub_key( argc, argv );
  } else if ( cmd == "get_product" ) {
    rc = on_get_product( argc, argv );
  } else if ( cmd == "get_product_list" ) {
    rc = on_get_product_list( argc, argv );
  } else if ( cmd == "get_block" ) {
    rc = on_get_block( argc, argv );
  } else if ( cmd == "version" ) {
    std::cout << "version: " << PC_VERSION << std::endl;
  } else {
    std::cerr << "pyth: unknown command" << std::endl;
    rc = usage();
  }
  return rc;
}
