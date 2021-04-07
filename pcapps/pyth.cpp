#include <pc/manager.hpp>
#include <pc/log.hpp>
#include <pc/misc.hpp>
#include <unistd.h>
#include <signal.h>
#include <iostream>

// pyth command-line tool

int get_exponent()
{
  return -4;
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
  std::cerr << "  init_key        [options]" << std::endl;
  std::cerr << "  init_program    [options]" << std::endl;
  std::cerr << "  init_mapping    [options]" << std::endl;
  std::cerr << "  transfer        <pub_key> <amount> [options]"
            << std::endl;
  std::cerr << "  add_symbol      <symbol> <price_type> "
            << "[-e <price_exponent (default " << get_exponent()
            << ")>] [options]" << std::endl;
  std::cerr << "  add_publisher   <pub_key> <symbol> <price_type> [options]"
            << std::endl;
  std::cerr << "  del_publisher   <pub_key> <symbol> <price_type> [options]"
            << std::endl;
  std::cerr << "  get_balance     [<pub_key>] [options]" << std::endl;
  std::cerr << "  get_symbol      <symbol> <price_type> [options]"
            << std::endl;
  std::cerr << "  get_symbol_list [options]" << std::endl;
  std::cerr << "  get_pub_key     <key_pair_file>" << std::endl;
  std::cerr << std::endl;

  std::cerr << "options include:" << std::endl;
  std::cerr << "  -r <rpc_host (default " << get_rpc_host() << ")>"
             << std::endl;
  std::cerr << "  -k <key_store_directory (default "
            << get_key_store() << ">" << std::endl;
  std::cerr << "  -c <commitment_level (default confirmed)>" << std::endl;
  return 1;
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

int submit_request( const std::string& rpc_host,
                    const std::string& key_dir,
                    request *req )
{
  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  return submit_request( mgr, req );
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
  std::cout << "this might take take up to 30 seconds..." << std::endl;

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
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
  pub.init_from_text( argv[1] );
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
  std::cout << "  account    : " << knm << std::endl
            << "  amount     : " << lamports/1e9 << std::endl;
  std::cout << "are you sure? [y/n] ";
  char ch;
  std::cin >> ch;
  if ( ch != 'y' && ch != 'Y' ) {
    return 1;
  }

  // submit request
  transfer req[1];
  req->set_lamports( lamports );
  req->set_commitment( cmt );
  req->set_receiver( &pub );
  return submit_request( rpc_host, key_dir, req );
}

int on_get_balance( int argc, char **argv )
{
  pub_key pub;
  std::string knm;
  if ( argc > 1 && argv[1][0] != '-' ) {
    knm = argv[1];
  }
  int opt = 0;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "r:k:p:h" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'p': knm = optarg; break;
      default: return usage();
    }
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
  balance req[1];
  req->set_pub_key( &pub );
  int ret = submit_request( rpc_host, key_dir, req );
  if ( ret == 0 ) {
    pub.enc_base58( knm );
    std::cout << "account: " << knm << std::endl;
    printf( "balance: %.9f\n", 1e-9*req->get_lamports() );
  }
  return ret;
}

int on_add_symbol( int argc, char **argv )
{
  // get input parameters
  if ( argc < 3 ) {
    return usage();
  }
  str sym_s( argv[1] );
  if ( sym_s.len_ > symbol::len ) {
    std::cerr << "pyth: symbol length too long" << std::endl;
    return usage();
  }
  symbol sym( sym_s );
  if ( sym.data()[0] == 0x20 ) {
    return usage();
  }
  price_type ptype = str_to_price_type( argv[2] );
  if ( ptype == price_type::e_unknown ) {
    std::cerr << "pyth: unknown price_type=" << argv[3] << std::endl;
    return usage();
  }
  argc -= 1;
  argv += 1;
  int opt = 0, exponent = get_exponent();
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  commitment cmt = commitment::e_confirmed;
  while( (opt = ::getopt(argc,argv, "e:r:k:c:dh" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'c': cmt = str_to_commitment(optarg); break;
      case 'd': log::set_level( PC_LOG_DBG_LVL ); break;
      case 'e': exponent = ::atoi( optarg ); break;
      default: return usage();
    }
  }
  if ( cmt == commitment::e_unknown ) {
    std::cerr << "pyth: unknown commitment level" << std::endl;
    return usage();
  }

  // are you sure prompt
  str pstr = price_type_to_str( ptype );
  std::cout << "adding new symbol account:" << std::endl;
  std::cout << "  symbol     : ";
  std::cout.write( sym.data(), symbol::len );
  std::cout << std::endl;
  std::cout << "  price_type : ";
  std::cout.write( pstr.str_, pstr.len_ );
  std::cout << std::endl
            << "  version    : " << PC_VERSION << std::endl
            << "  exponent   : " << exponent << std::endl;
  std::cout << "are you sure? [y/n] ";
  char ch;
  std::cin >> ch;
  if ( ch != 'y' && ch != 'Y' ) {
    return 1;
  }
  std::cout << "this might take take up to 30 seconds..." << std::endl;

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }

  // check if we need to create a new mapping account first
  get_mapping *mptr = mgr.get_last_mapping();
  if ( mptr && mptr->get_is_full() ) {
    // get rent-exemption amount for mapping account
    get_minimum_balance_rent_exemption req_r[1];
    req_r->set_size( sizeof( pc_map_node_t ) );
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
  req_r->set_size( sizeof( pc_price_t ) );
  if( submit_request( mgr, req_r ) ) {
    return 1;
  }
  // add new symbol account
  add_symbol req_a[1];
  req_a->set_symbol( sym );
  req_a->set_exponent( exponent );
  req_a->set_price_type( ptype );
  req_a->set_lamports( req_r->get_lamports() );
  req_a->set_commitment( cmt );
  if( submit_request( mgr, req_a ) ) {
    return 1;
  }
  return 0;
}

int on_get_symbol( int argc, char **argv )
{
  // get input parameters
  if ( argc < 3 ) {
    return usage();
  }
  str sym_s( argv[1] );
  if ( sym_s.len_ > symbol::len ) {
    std::cerr << "pyth: symbol length too long" << std::endl;
    return usage();
  }
  symbol sym( sym_s );
  if ( sym.data()[0] == 0x20 ) {
    return usage();
  }
  price_type ptype = str_to_price_type( argv[2] );
  if ( ptype == price_type::e_unknown ) {
    std::cerr << "pyth: unknown price_type=" << argv[3] << std::endl;
    return usage();
  }
  argc -= 1;
  argv += 1;
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

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  // get rent exemption amount
  get_minimum_balance_rent_exemption req[1];
  req->set_size( sizeof( pc_price_t ) );
  if( submit_request( mgr, req ) ) {
    return 1;
  }

  // print symbol details
  str pstr = price_type_to_str( ptype );
  price *ptr = mgr.get_symbol( sym, ptype );
  if ( !ptr ) {
    std::cerr << "pyth: cannot find symbol=";
    std::cerr.write( sym.data(), symbol::len );
    std::cerr << ",price_type=";
    std::cerr.write( pstr.str_, pstr.len_ );
    std::cerr << std::endl;
    return 1;
  }
  std::string pkey;
  ptr->get_account()->enc_base58( pkey );
  std::cout << "symbol    : ";
  std::cout.write( sym.data(), symbol::len );
  std::cout << std::endl;
  std::cout << "price_type: ";
  std::cout.write( pstr.str_, pstr.len_ );
  std::cout << std::endl;
  std::cout << "account   : " << pkey << std::endl;
  printf( "balance   : %.9f\n", 1e-9*ptr->get_lamports() );
  printf( "rent      : %.9f\n", 1e-9*req->get_lamports() );
  std::cout << "price     : " << ptr->get_price() << std::endl;
  std::cout << "conf      : " << ptr->get_conf() << std::endl;
  std::cout << "exponent  : " << ptr->get_price_exponent() << std::endl;
  std::cout << "valid_slot: " << ptr->get_valid_slot() << std::endl;
  std::cout << "pub_slot  : " << ptr->get_pub_slot() << std::endl;
  for( unsigned i=0; i != ptr->get_num_publisher(); ++i ) {
    const pub_key*ipub = ptr->get_publisher( i );
    std::string istr;
    ipub->enc_base58( istr );
    std::cout << "publisher : " << istr << std::endl;
    std::cout << "  price   : " << ptr->get_publisher_price(i) << std::endl;
    std::cout << "  conf    : " << ptr->get_publisher_conf(i) << std::endl;
  }
  return 0;
}

int on_get_symbol_list( int argc, char **argv )
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

  // initialize connection to block-chain
  manager mgr;
  mgr.set_rpc_host( rpc_host );
  mgr.set_dir( key_dir );
  if ( !mgr.init() || !mgr.bootstrap() ) {
    std::cerr << "pyth: " << mgr.get_err_msg() << std::endl;
    return 1;
  }
  for(unsigned i=0; i != mgr.get_num_symbol(); ++i ) {
    price *ptr = mgr.get_symbol( i );
    symbol *sym = ptr->get_symbol();
    price_type ptype = ptr->get_price_type();
    str pstr = price_type_to_str( ptype );
    std::cout.write( sym->data(), symbol::len );
    std::cout << ' ';
    std::cout.write( pstr.str_, pstr.len_ );
    std::cout << std::endl;
  }
  return 0;
}

int on_add_publisher( int argc, char **argv )
{
  // get input parameters
  if ( argc < 4 ) {
    return usage();
  }
  str sym_s( argv[2] );
  if ( sym_s.len_ > symbol::len ) {
    std::cerr << "pyth: symbol length too long" << std::endl;
    return usage();
  }
  pub_key pub;
  pub.init_from_text( argv[1] );
  symbol  sym( sym_s );
  price_type ptype = str_to_price_type( argv[3] );
  if ( ptype == price_type::e_unknown ) {
    std::cerr << "pyth: unknown price_type=" << argv[3] << std::endl;
    return usage();
  }
  argc -= 3;
  argv += 3;
  int opt = 0;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  commitment cmt = commitment::e_confirmed;
  while( (opt = ::getopt(argc,argv, "e:r:k:c:dh" )) != -1 ) {
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

  // are you sure prompt
  std::string pnm;
  pub.enc_base58( pnm );
  str pstr = price_type_to_str( ptype );
  std::cout << "adding new symbol publisher:" << std::endl;
  std::cout << "  symbol    : ";
  std::cout.write( sym.data(), symbol::len );
  std::cout << std::endl;
  std::cout << "  price_type: ";
  std::cout.write( pstr.str_, pstr.len_ );
  std::cout << std::endl;
  std::cout << "  version   : " << PC_VERSION << std::endl;
  std::cout << "  publisher : " << pnm << std::endl;
  std::cout << "are you sure? [y/n] ";
  char ch;
  std::cin >> ch;
  if ( ch != 'y' && ch != 'Y' ) {
    return 1;
  }

  // submit request
  add_publisher req[1];
  req->set_symbol( sym );
  req->set_publisher( pub );
  req->set_price_type( ptype );
  req->set_commitment( cmt );
  return submit_request( rpc_host, key_dir, req );
}

int on_del_publisher( int argc, char **argv )
{
  // get input parameters
  if ( argc < 4 ) {
    return usage();
  }
  str sym_s( argv[2] );
  if ( sym_s.len_ > symbol::len ) {
    std::cerr << "pyth: symbol length too long" << std::endl;
    return usage();
  }
  pub_key pub;
  pub.init_from_text( argv[1] );
  symbol  sym( sym_s );
  price_type ptype = str_to_price_type( argv[3] );
  if ( ptype == price_type::e_unknown ) {
    std::cerr << "pyth: unknown price_type=" << argv[3] << std::endl;
    return usage();
  }
  argc -= 3;
  argv += 3;
  int opt = 0;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  commitment cmt = commitment::e_confirmed;
  while( (opt = ::getopt(argc,argv, "e:r:k:c:dh" )) != -1 ) {
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

  // are you sure prompt
  std::string pnm;
  pub.enc_base58( pnm );
  str pstr = price_type_to_str( ptype );
  std::cout << "removing publisher:" << std::endl;
  std::cout << "  symbol    : ";
  std::cout.write( sym.data(), symbol::len );
  std::cout << std::endl;
  std::cout << "  price_type: ";
  std::cout.write( pstr.str_, pstr.len_ );
  std::cout << std::endl;
  std::cout << "  version   : " << PC_VERSION << std::endl;
  std::cout << "  publisher : " << pnm << std::endl;
  std::cout << "are you sure? [y/n] ";
  char ch;
  std::cin >> ch;
  if ( ch != 'y' && ch != 'Y' ) {
    return 1;
  }

  // submit request
  del_publisher req[1];
  req->set_symbol( sym );
  req->set_publisher( pub );
  req->set_price_type( ptype );
  req->set_commitment( cmt );
  return submit_request( rpc_host, key_dir, req );
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
  } else if ( cmd == "transfer" ) {
    rc = on_transfer( argc, argv );
  } else if ( cmd == "get_balance" ) {
    rc = on_get_balance( argc, argv );
  } else if ( cmd == "add_symbol" ) {
    rc = on_add_symbol( argc, argv );
  } else if ( cmd == "get_symbol" ) {
    rc = on_get_symbol( argc, argv );
  } else if ( cmd == "get_symbol_list" ) {
    rc = on_get_symbol_list( argc, argv );
  } else if ( cmd == "add_publisher" ) {
    rc = on_add_publisher( argc, argv );
  } else if ( cmd == "del_publisher" ) {
    rc = on_del_publisher( argc, argv );
  } else if ( cmd == "get_pub_key" ) {
    rc = on_get_pub_key( argc, argv );
  } else {
    std::cerr << "pyth: unknown command" << std::endl;
    rc = usage();
  }
  return rc;
}
