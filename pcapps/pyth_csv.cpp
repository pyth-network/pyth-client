#include <pc/replay.hpp>
#include <pc/rpc_client.hpp>
#include <pc/misc.hpp>
#include <unistd.h>
#include <signal.h>
#include <iostream>

using namespace pc;

// pyth csv playback utility
class csv_print
{
public:

  csv_print();

  // print csv header row
  void print_header();

  // set symbol to filter on
  void set_symbol( const std::string& );

  // parse next update
  void parse( replay& );

private:

  struct trait_account {
    static const size_t hsize_ = 8363UL;
    typedef uint32_t     idx_t;
    typedef pub_key      key_t;
    typedef const key_t& keyref_t;
    typedef std::string  val_t;
    struct hash_t {
      idx_t operator() ( keyref_t a ) {
        uint64_t *p = (uint64_t*)a.data();
        return p[0] ^ p[1];
      }
    };
  };

  typedef hash_map<trait_account> symbol_map_t;

  void parse_product( replay& );
  void parse_price( replay& );

  symbol_map_t smap_;
  attr_id      sym_id_;
  attr_dict    attr_;
  pub_key      skey_;
  std::string  sym_;
  bool         do_sym_;
  bool         has_sym_;
};

csv_print::csv_print()
: sym_id_( attr_id::add( "symbol" ) ),
  do_sym_( false ),
  has_sym_( false )
{
}

void csv_print::set_symbol( const std::string& sym )
{
  sym_ = sym;
  do_sym_ = !sym_.empty();
}

void csv_print::print_header()
{
  std::cout << "time"
            << ",symbol"
            << ",price_type"
            << ",price_exponent"
            << ",status"
            << ",price"
            << ",conf"
            << ",twap"
            << ",twac"
            << ",valid_slot"
            << ",pub_slot"
            << ",prev_slot"
            << ",prev_price"
            << ",prev_conf";
  for(unsigned i=0; i != PC_COMP_SIZE; ++i ) {
    std::cout << ",comp_status_" << i
              << ",comp_price_" << i
              << ",comp_conf_" << i
              << ",comp_slot" << i;
  }
  std::cout << std::endl;
}

void csv_print::parse_product( replay& rep )
{
  // generate attribute dictionary from account and get symbol
  str sym, val;
  if ( !attr_.init_from_account( (pc_prod_t*)rep.get_update() ) ||
       !attr_.get_attr( sym_id_, sym ) ) {
    return;
  }

  // generate map from product account to string symbol
  pub_key *aptr = (pub_key*)rep.get_account();
  symbol_map_t::iter_t i = smap_.find( *aptr );
  if ( !i ) {
    i = smap_.add( *aptr );
  }
  smap_.ref( i ) = sym.as_string();

  // check if symbol matches filter
  if ( do_sym_ && sym_ == sym.as_string() ) {
    has_sym_ = true;
    skey_ = *aptr;
  }
}

void csv_print::parse_price( replay& rep )
{
  pc_price_t *ptr = (pc_price_t*)rep.get_update();
  pub_key *aptr = (pub_key*)&ptr->prod_;
  // filter by symbol
  if ( do_sym_ && (!has_sym_ || *aptr != skey_ ) ) {
    return;
  }
  char tbuf[32];
  nsecs_to_utc6( rep.get_time(), tbuf );
  std::cout.write( tbuf, 27 );
  std::cout << ',';
  symbol_map_t::iter_t i = smap_.find( *aptr );
  if ( i ) std::cout << smap_.obj(i);
  std::cout << ',';
  str pstr = price_type_to_str( (price_type)ptr->ptype_ );
  std::cout.write( pstr.str_, pstr.len_ );
  std::cout << ',' << ptr->expo_;
  std::cout << ',';
  str sstr = symbol_status_to_str( (symbol_status)ptr->agg_.status_ );
  std::cout.write( sstr.str_, sstr.len_ );
  std::cout << ',' << ptr->agg_.price_
            << ',' << ptr->agg_.conf_
            << ',' << ptr->twap_.val_
            << ',' << ptr->twac_.val_
            << ',' << ptr->valid_slot_
            << ',' << ptr->agg_.pub_slot_
            << ',' << ptr->prev_slot_
            << ',' << ptr->prev_price_
            << ',' << ptr->prev_conf_;
  for( unsigned i=0; i != ptr->num_; ++i ) {
    pc_price_comp_t *cptr = &ptr->comp_[i];
    std::cout << ',';
    str sstr = symbol_status_to_str( (symbol_status)cptr->agg_.status_ );
    std::cout.write( sstr.str_, sstr.len_ );
    std::cout << ',' << cptr->agg_.price_
              << ',' << cptr->agg_.conf_
              << ',' << cptr->agg_.pub_slot_;
  }
  for( unsigned i=ptr->num_; i != PC_COMP_SIZE; ++i ) {
    std::cout << ",,,,";
  }
  std::cout << std::endl;
}

void csv_print::parse( replay& rep )
{
  pc_acc_t *ptr = rep.get_update();
  switch( ptr->type_ ) {
    case PC_ACCTYPE_PRICE:   parse_price( rep ); break;
    case PC_ACCTYPE_PRODUCT: parse_product( rep ); break;
    case PC_ACCTYPE_MAPPING: break;
  }
}

int usage()
{
  std::cerr << "usage: pyth_csv <cap_file> [options]"
            << std::endl << std::endl;
  std::cerr << "options include:" << std::endl;
  std::cerr << "  -s <symbol>" << std::endl;
  return 1;
}


int main(int argc, char **argv)
{
  // get input params and options
  if ( argc < 2 ) {
    return usage();
  }
  int opt = 0;
  std::string cap_file = argv[1], symstr;
  argc -= 1;
  argv += 1;
  while( (opt = ::getopt(argc,argv, "s:h" )) != -1 ) {
    switch(opt) {
      case 's': symstr = optarg; break;
      default: return usage();
    }
  }
  // initialize replay api
  replay rep;
  rep.set_file( cap_file );
  if ( !rep.init() ) {
    std::cerr << "pyth_csv: " << rep.get_err_msg() << std::endl;
    return 1;
  }

  // csv print object
  csv_print csv;
  csv.print_header();
  csv.set_symbol( symstr );

  // loop through and parse all updates in capture
  for(;;) {
    if ( rep.get_next() ) {
      csv.parse( rep );
    } else {
      break;
    }
  }
  return 0;
}
