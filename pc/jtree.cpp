#include "jtree.hpp"
#include <ctype.h>
#include <stdlib.h>

using namespace pc;

jtree::jtree()
: key_( 0 ), buf_( nullptr  )
{
}

void jtree::parse( const char *cptr, size_t sz )
{
  buf_ = cptr;
  key_ = 0;
  nv_.resize(1);
  st_.clear();

  typedef enum { e_start, e_string, e_number, e_keyword } state_t;
  state_t st = e_start;
  const char *txt, *end = &cptr[sz];
  for(;;) {
    switch(st) {
      case e_start: {
        for(;;++cptr) {
          if( cptr==end ) return;
          if (*cptr == '{' ) {
            parse_start_object();
          } else if ( *cptr == '[' ) {
            parse_start_array();
          } else if (*cptr == '}' ) {
            parse_end_object();
          } else if ( *cptr == ']' ) {
            parse_end_array();
          } else if ( *cptr == '"' ) {
            st = e_string; txt=cptr+1; break;
          } else if ( *cptr == '-' || *cptr == '.' || isdigit(*cptr)) {
            st = e_number; txt=cptr; break;
          } else if ( *cptr == 't' || *cptr == 'f' || *cptr == 'n' ) {
            st = e_keyword; txt=cptr; break;
          }
        }
        ++cptr;
        break;
      }

      case e_string: {
        for(;;++cptr) {
          if( cptr==end ) return;
          if (*cptr == '"' ) {
            st = e_start;
            const char *etxt = cptr;
            // peek if this is a key
            for(++cptr;;++cptr) {
              if( cptr==end ) return;
              if ( *cptr == ':' ) {
                parse_key( txt, etxt );
                break;
              } else if ( !isspace(*cptr) ) {
                parse_string( txt, etxt );
                break;
              }
            }
            break;
          }
        }
        break;
      }

      case e_number: {
        for(;;++cptr) {
          if( cptr==end ) return;
          if ( !isdigit(*cptr) && *cptr!='.' &&
               *cptr!='-' && *cptr!='e' && *cptr!='+' ) {
            st = e_start; parse_number( txt, cptr );break;
          }
        }
        break;
      }

      case e_keyword:{
        for(;;++cptr) {
          if ( cptr==end ) return;
          if (!isalpha(*cptr) ) {
            st = e_start; parse_keyword( txt, cptr ); break;
          }
        }
        break;
      }
    }
  }
}

bool jtree::is_valid() const
{
  return nv_.size()>1 && st_.empty();
}

uint32_t jtree::new_node(type_t t, uint32_t i, uint32_t j )
{
  uint32_t nxt = nv_.size();
  nv_.resize( 1 + nxt );
  node& nd = nv_[nxt];
  nd.type_ = t;
  nd.next_ = 0;
  nd.h_    = i;
  nd.t_    = j;
  return nxt;
}

void jtree::add_obj( uint32_t val )
{
  uint32_t kvidx = new_node( e_keyval, key_, val );
  add_arr( kvidx );
}

void jtree::add_arr( uint32_t val )
{
  node& obj = nv_[st_.back()];
  if ( obj.t_ ) {
    nv_[obj.t_].next_ = val;
    obj.t_ = val;
  } else {
    obj.h_ = obj.t_ = val;
  }
}

void jtree::add( uint32_t val )
{
  if ( !st_.empty() ) {
    if ( key_ ) {
      add_obj( val );
      key_ = 0;
    } else {
      add_arr( val );
    }
  }
}

void jtree::parse_start_object()
{
  uint32_t idx = new_node( e_obj, 0, 0 );
  add( idx );
  st_.push_back( idx );
}

void jtree::parse_start_array()
{
  uint32_t idx = new_node( e_arr, 0, 0 );
  add( idx );
  st_.push_back( idx );
}

void jtree::parse_end_object()
{
  st_.pop_back();
}

void jtree::parse_end_array()
{
  st_.pop_back();
}

void jtree::parse_key( const char *txt, const char *end )
{
  key_ = new_node( e_val, txt-buf_, end-txt );
}

void jtree::parse_number( const char *txt, const char *end)
{
  parse_string( txt, end );
}

void jtree::parse_string( const char *txt, const char *end)
{
  add( new_node( e_val, txt-buf_, end-txt ) );
}

void jtree::parse_keyword( const char *txt, const char *end)
{
  parse_string( txt, end );
}

uint32_t jtree::find_val( uint32_t obj, str key ) const
{
  for(uint32_t it=get_first(obj); it; it = get_next(it) ) {
    if ( e_keyval == get_type( it ) ) {
      if ( key == get_str( get_key( it ) ) ) {
        return get_val( it );
      }
    } else {
      break;
    }
  }
  return 0;
}
