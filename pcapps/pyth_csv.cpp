#include <pc/replay.hpp>
#include <pc/rpc_client.hpp>
#include <pc/misc.hpp>
#include <unistd.h>
#include <signal.h>
#include <iostream>

// pyth csv playback utility

using namespace pc;

int usage()
{
  std::cerr << "usage: pyth_csv <cap_file> [options]"
            << std::endl << std::endl;
  std::cerr << "options include:" << std::endl;
  std::cerr << "  -s <symbol>" << std::endl;
  std::cerr << "  -f" << std::endl;
  return 1;
}

void print_header()
{
  std::cout << "time"
            << ",symbol"
            << ",price_type"
            << ",price_exponent"
            << ",status"
            << ",price"
            << ",conf"
            << ",valid_slot"
            << ",pub_slot";
  for(unsigned i=0; i != PC_COMP_SIZE; ++i ) {
    std::cout << ",comp_status_" << i
              << ",comp_price_" << i
              << ",comp_conf_" << i;
  }
  std::cout << std::endl;
}

void print( replay& rep )
{
  pc_price_t *ptr = rep.get_update();
  char tbuf[32];
  nsecs_to_utc6( rep.get_time(), tbuf );
  std::cout.write( tbuf, 27 );
  std::cout << ',';
  std::cout.write( (char*)ptr->sym_.k1_, PC_SYMBOL_SIZE );
  std::cout << ',';
  str pstr = price_type_to_str( (price_type)ptr->ptype_ );
  std::cout.write( pstr.str_, pstr.len_ );
  std::cout << ',';
  str sstr = symbol_status_to_str( (symbol_status)ptr->agg_.status_ );
  std::cout.write( sstr.str_, sstr.len_ );
  std::cout << ',' << ptr->expo_
            << ',' << ptr->agg_.price_
            << ',' << ptr->agg_.conf_
            << ',' << ptr->valid_slot_
            << ',' << ptr->agg_.pub_slot_;
  for( unsigned i=0; i != ptr->num_; ++i ) {
    pc_price_comp_t *cptr = &ptr->comp_[i];
    std::cout << ',';
    str sstr = symbol_status_to_str( (symbol_status)cptr->agg_.status_ );
    std::cout.write( sstr.str_, sstr.len_ );
    std::cout << ',' << ptr->agg_.price_
              << ',' << ptr->agg_.conf_;
  }
  for( unsigned i=ptr->num_; i != PC_COMP_SIZE; ++i ) {
    std::cout << ",,,";
  }
  std::cout << std::endl;
}

int main(int argc, char **argv)
{
  if ( argc < 2 ) {
    return usage();
  }
  int opt = 0;
  bool do_follow = false;
  std::string cap_file = argv[1], symstr;
  argc -= 1;
  argv += 1;
  while( (opt = ::getopt(argc,argv, "s:fh" )) != -1 ) {
    switch(opt) {
      case 's': symstr = optarg; break;
      case 'f': do_follow = true; break;
      default: return usage();
    }
  }
  replay rep;
  rep.set_file( cap_file );
  if ( !rep.init() ) {
    std::cerr << "pyth_csv: " << rep.get_err_msg() << std::endl;
    return 1;
  }
  struct timespec ts[1];
  ts->tv_sec  = 1;
  ts->tv_nsec = 0;
  print_header();
  if ( symstr.empty() ) {
    // print all updates
    for(;;) {
      if ( rep.get_next() ) {
        print( rep );
      } else if ( do_follow ) {
        clock_nanosleep( CLOCK_REALTIME, 0, ts, NULL );
      } else {
        break;
      }
    }
  } else {
    // filter by symbol
    symbol sym( symstr.c_str() );
    for(;;) {
      if ( rep.get_next() ) {
        pc_price_t *ptr = rep.get_update();
        symbol *sptr = (symbol*)&ptr->sym_;
        if ( sym == sptr[0] ) {
          print( rep );
        }
      } else if ( do_follow ) {
        clock_nanosleep( CLOCK_REALTIME, 0, ts, NULL );
      } else {
        break;
      }
    }
  }

  return 0;
}
