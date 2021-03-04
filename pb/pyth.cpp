#include <pc/pyth_client.hpp>
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
  std::cerr << "  init_key       [options]" << std::endl;
  std::cerr << "  init_mapping   <amount> [options]" << std::endl;
  std::cerr << "  add_mapping    <amount> [options]" << std::endl;
  std::cerr << "  add_symbol     <symbol> <amount> "
            << "[-e <price_exponent (default " << get_exponent()
            << ")>] [options]" << std::endl;
  std::cerr << "  add_publisher  <pub_key> <symbol> [options]" << std::endl;
  std::cerr << std::endl;
  std::cerr << "options include:" << std::endl;
  std::cerr << "  -r <rpc_host (default " << get_rpc_host() << ")>"
             << std::endl;
  std::cerr << "  -k <key_store_directory (default "
            << get_key_store() << ">]" << std::endl;
  return 1;
}

int submit_request( const std::string& rpc_host,
                    const std::string& key_dir,
                    pyth_request *req )
{
  // initialize connection to block-chain
  pyth_client clnt;
  clnt.set_rpc_host( rpc_host );
  clnt.set_dir( key_dir );
  if ( !clnt.init() ) {
    std::cerr << "pyth: " << clnt.get_err_msg() << std::endl;
    return 1;
  }
  // submit request and poll for completion or error
  clnt.submit( req );
  while( !req->get_is_done() &&
         !req->get_is_err() &&
         !clnt.get_is_err() ) {
    clnt.poll();
  }
  clnt.teardown();
  if ( req->get_is_err() ) {
    std::cerr << "pyth: " << req->get_err_msg() << std::endl;
    return 1;
  }
  if ( clnt.get_is_err() ) {
    std::cerr << "pyth: " << clnt.get_err_msg() << std::endl;
    return 1;
  }
  return 0;
}

int init_key( int argc, char **argv )
{
  int opt = 0;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "r:k:h" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      default: return usage();
    }
  }
  key_store kst;
  kst.set_dir( key_dir );
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

int init_mapping( int argc, char **argv )
{
  // get input parameters
  if ( argc < 2 ) {
    return usage();
  }
  int64_t lamports = str_to_dec( argv[1], -9 );
  if ( lamports <= 0 ) {
    return usage();
  }
  --argc;
  ++argv;
  int opt = 0;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "r:k:h" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      default: return usage();
    }
  }

  // submit request
  pyth_init_mapping req[1];
  req->set_lamports( lamports );
  return submit_request( rpc_host, key_dir, req );
}

int add_symbol( int argc, char **argv )
{
  // get input parameters
  if ( argc < 3 ) {
    return usage();
  }
  if ( __builtin_strlen(argv[1]) > symbol::len ) {
    std::cerr << "pyth: symbol length too long" << std::endl;
    return usage();
  }
  symbol sym( argv[1] );
  if ( !*sym.data() ) {
    return usage();
  }
  int64_t lamports = str_to_dec( argv[2], -9 );
  if ( lamports <= 0 ) {
    return usage();
  }
  argc -= 2;
  argv += 2;
  int opt = 0, exponent = get_exponent();
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "e:r:k:h" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      case 'e': exponent = ::atoi( optarg ); break;
      default: return usage();
    }
  }
  // are you sure prompt
  std::cout << "adding new symbol account:" << std::endl;
  std::cout << "  symbol  : ";
  std::cout.write( sym.data(), symbol::len );
  std::cout << std::endl
            << "  amount  : " << lamports/1e9 << std::endl
            << "  exponent: " << exponent
            << std::endl;
  std::cout << "are you sure? [y/n] ";
  char ch;
  std::cin >> ch;
  if ( ch != 'y' && ch != 'Y' ) {
    return 1;
  }

  // submit request
  pyth_add_symbol req[1];
  req->set_symbol( sym );
  req->set_exponent( exponent );
  req->set_lamports( lamports );
  return submit_request( rpc_host, key_dir, req );
}

int add_publisher( int argc, char **argv )
{
  // get input parameters
  if ( argc < 3 ) {
    return usage();
  }
  if ( __builtin_strlen(argv[2]) > symbol::len ) {
    std::cerr << "pyth: symbol length too long" << std::endl;
    return usage();
  }
  pub_key pub;
  pub.init_from_text( argv[1] );
  symbol  sym( argv[2] );

  argc -= 2;
  argv += 2;
  int opt = 0;
  std::string rpc_host = get_rpc_host();
  std::string key_dir  = get_key_store();
  while( (opt = ::getopt(argc,argv, "e:r:k:h" )) != -1 ) {
    switch(opt) {
      case 'r': rpc_host = optarg; break;
      case 'k': key_dir = optarg; break;
      default: return usage();
    }
  }
  // are you sure prompt
  std::string pnm;
  pub.enc_base58( pnm );
  std::cout << "adding new symbol publisher:" << std::endl;
  std::cout << "  symbol   : ";
  std::cout.write( sym.data(), symbol::len );
  std::cout << std::endl;
  std::cout << "  publisher: " << pnm << std::endl;
  std::cout << "are you sure? [y/n] ";
  char ch;
  std::cin >> ch;
  if ( ch != 'y' && ch != 'Y' ) {
    return 1;
  }

  // submit request
  pyth_add_publisher req[1];
  req->set_symbol( sym );
  req->set_publisher( pub );
  return submit_request( rpc_host, key_dir, req );
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
  log::set_level( PC_LOG_INF_LVL );

  // dispatch by command
  const char *cmd = argv[0];
  int rc = 0;
  if ( 0 == __builtin_strcmp( cmd, "init_key" ) ) {
    rc = init_key( argc, argv );
  } else if ( 0 == __builtin_strcmp( cmd, "init_mapping" ) ) {
    rc = init_mapping( argc, argv );
  } else if ( 0 == __builtin_strcmp( cmd, "add_symbol" ) ) {
    rc = add_symbol( argc, argv );
  } else if ( 0 == __builtin_strcmp( cmd, "add_publisher" ) ) {
    rc = add_publisher( argc, argv );
  } else {
    rc = usage();
  }
  return rc;
}
