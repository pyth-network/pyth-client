char heap_start[8192];
#define PC_HEAP_START (heap_start)

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <oracle/oracle.h>
#include <oracle/upd_aggregate.h>
#include <pc/mem_map.hpp>
#include <math.h>
#include <ctype.h>
#include <iostream>

using namespace pc;

int usage()
{
  std::cerr << "usage: test_twap <twap.csv>" << std::endl;
  return 1;
}

class csv_parser
{
public:
  csv_parser( const char *buf, size_t len )
  : ptr_( buf ),
    eol_( buf ),
    end_( &buf[len] ),
    first_( false ) {
  }
  bool fetch_line() {
    for( ptr_ = eol_;;++ptr_ ) {
      if ( ptr_ == end_ ) return false;
      if ( *ptr_ != '\n' && *ptr_ != '\r' ) break;
    }
    for( eol_ = ptr_;ptr_ != end_ && *eol_ != '\n' && *eol_ != '\r';++eol_ );
    first_ = true;
    return true;
  }
  void fetch( int64_t& val ) {
    if ( ptr_ != eol_ && *ptr_ == ',' && !first_ ) {
      ++ptr_;
    }
    first_ = false;
    const char *ptxt = ptr_;
    for( ; ptr_ != eol_ && *ptr_ != ','; ++ptr_ );
    char *etxt[1];
    etxt[0] = (char*)ptr_;
    val = strtol( ptxt, etxt, 10 );
  }
private:
  const char *ptr_;
  const char *eol_;
  const char *end_;
  bool        first_;
};

int main( int argc,char** argv )
{
  if ( argc < 2 ) {
    return usage();
  }

  // read price file
  mem_map mf;
  std::string file = argv[1];
  mf.set_file(file );
  if ( !mf.init() ) {
    std::cerr << "test_qset: failed to read file[" << file << "]"
              << std::endl;
    return 1;
  }
  pc_price_t px[1];
  __builtin_memset( px, 0, sizeof( pc_price_t ) );
  uint64_t slot = 1000;
  px->last_slot_ = slot;
  px->agg_.pub_slot_ = slot;
  px->num_  = 0;
  upd_aggregate( px, slot+1 );
  pc_qset_t *qs = qset_new( px->expo_ );

  // skip first line
  csv_parser cp( mf.data(), mf.size( ));

  // update twap for each update
  std::cout << "price,conf,expo,nslots,twap,twac" << std::endl;
  for( cp.fetch_line(); cp.fetch_line(); ) {
    int64_t nslots = 0, price = 0, conf = 0, expo = 0;
    cp.fetch( price );
    cp.fetch( conf );
    cp.fetch( expo );
    cp.fetch( nslots );
    px->expo_ = expo;
    px->agg_.price_ = price;
    px->agg_.conf_  = conf;
    upd_twap( px, nslots, qs );
    std::cout << price << ','
              << conf << ','
              << expo << ','
              << nslots << ','
              << px->twap_.val_ << ','
              << px->twac_.val_
              << std::endl;
  }

  return 0;
}
