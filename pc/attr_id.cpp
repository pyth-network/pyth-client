#include "attr_id.hpp"

using namespace pc;

///////////////////////////////////////////////////////////////////////////
// attr_dict

attr_dict::pos::pos()
: idx_( 0 ), len_ ( 0 ) {
}

void attr_dict::clear()
{
  num_ = 0;
  abuf_.clear();
  avec_.clear();
}

unsigned attr_dict::get_num_attr() const
{
  return num_;
}

bool attr_dict::get_attr( attr_id aid, str& val ) const
{
  if ( aid.get_id() >= avec_.size()  ) return false;
  const pos& p = avec_[aid.get_id()];
  val.str_ = &abuf_[p.idx_];
  val.len_ = p.len_;
  return val.len_ != 0;
}

bool attr_dict::get_next_attr( attr_id&id, str& val ) const
{
  for(unsigned i=1+id.get_id(); i<avec_.size(); ++i ) {
    id = attr_id( i );
    if ( get_attr( id, val ) ) {
      return true;
    }
  }
  return false;
}

void attr_dict::add_ref( attr_id aid, str val )
{
  if ( aid.get_id() >= avec_.size() ) {
    avec_.resize( 1 + aid.get_id() );
  }
  pos& p = avec_[aid.get_id()];
  p.idx_ = abuf_.size();
  p.len_ = val.len_;
  abuf_.resize( abuf_.size() + p.len_ );
  __builtin_memcpy( &abuf_[p.idx_], val.str_, p.len_ );
  ++num_;
}

bool attr_dict::init_from_account( pc_prod_t *aptr )
{
  clear();
  str key, val;
  char *ptr = (char*)aptr + sizeof( pc_prod_t );
  char *end = (char*)aptr + aptr->size_;
  while( ptr != end ) {
    key.str_ = &ptr[1];
    key.len_ = (size_t)ptr[0];
    ptr += 1 + key.len_;
    if ( ptr > end ) return false;
    val.str_ = &ptr[1];
    val.len_ = (size_t)ptr[0];
    ptr += 1 + val.len_;
    if ( ptr > end ) return false;
    attr_id aid = attr_id_set::inst().add_attr_id( key );
    add_ref( aid, val );
  }
  return true;
}

bool attr_dict::init_from_json( jtree& pt, uint32_t hd )
{
  if ( hd == 0 || pt.get_type( hd ) != jtree::e_obj ) {
    return false;
  }
  clear();
  for( uint32_t it=pt.get_first(hd); it; it = pt.get_next(it) ) {
    uint32_t kt = pt.get_key( it );
    if ( !kt ) return false;
    attr_id aid = attr_id::add( pt.get_str( kt ) );
    str val = pt.get_str( pt.get_val( it ) );
    add_ref( aid, val );
  }
  return true;
}

void attr_dict::write_account( net_wtr& wtr )
{
  str vstr, kstr;
  for( unsigned id=1; id < avec_.size(); ++id ) {
    attr_id aid( id );
    if ( !get_attr( aid, vstr ) ) {
      continue;
    }
    kstr = aid.get_str();
    wtr.add( (char)kstr.len_ );
    wtr.add( kstr );
    wtr.add( (char)vstr.len_ );
    wtr.add( vstr );
  }
}

void attr_dict::write_json( json_wtr& wtr ) const
{
  str vstr, kstr;
  for( unsigned id=1; id < avec_.size(); ++id ) {
    attr_id aid( id );
    if ( !get_attr( aid, vstr ) ) {
      continue;
    }
    kstr = aid.get_str();
    wtr.add_key( kstr, vstr );
  }
}

///////////////////////////////////////////////////////////////////////////
// attr_id_set

attr_id_set::attr_id_set()
: aid_( 0 )
{
}

str attr_id_set::attr_wtr::add_attr( str v )
{
  char *tgt = reserve( v.len_ );
  __builtin_memcpy( tgt, v.str_, v.len_ );
  advance( v.len_ );
  return str( tgt, v.len_ );
}

attr_id attr_id_set::add_attr_id( str v )
{
  attr_map_t::iter_t it = amap_.find( v );
  if ( it ) {
    return amap_.obj( it );
  } else {
    str k = abuf_.add_attr( v );
    it = amap_.add( k );
    attr_id aid( ++aid_ );
    avec_.resize( 1 + aid_ );
    avec_[aid_] = k;
    amap_.ref( it ) = aid;
    return aid;
  }
}

attr_id attr_id_set::get_attr_id( str v )
{
  attr_map_t::iter_t it = amap_.find( v );
  if ( it ) {
    return amap_.obj( it );
  } else {
    return attr_id();
  }
}

str attr_id_set::get_str( attr_id aid )
{
  if ( aid.get_id() < avec_.size() ) {
    return avec_[aid.get_id()];
  } else {
    return str();
  }
}
