#pragma once

#include <pc/misc.hpp>
#include <vector>
#include <stdint.h>

namespace pc
{
  // light-weight json parse tree
  class jtree
  {
  public:

    typedef enum {
      e_obj = 0,
      e_arr,
      e_keyval,
      e_val
    } type_t;

    jtree();

    // parse message
    void parse( const char *, size_t );
    bool is_valid() const;

    // get first element in tree
    type_t   get_type( uint32_t ) const;
    uint32_t get_next( uint32_t ) const;

    // get head of obj/arr
    uint32_t get_first( uint32_t ) const;
    uint32_t get_last( uint32_t ) const;

    // gey key/value pair
    uint32_t get_key( uint32_t ) const;
    uint32_t get_val( uint32_t ) const;

    // get primitive value
    void get_text( uint32_t, const char *&ptr, size_t& sz ) const;
    uint64_t get_uint( uint32_t ) const;
    int64_t  get_int( uint32_t ) const;
    bool     get_bool( uint32_t ) const;
    str      get_str( uint32_t ) const;

    // find value in object associated with key
    uint32_t find_val( uint32_t obj, const char *key ) const;

  public:

    void parse_start_object();
    void parse_start_array();
    void parse_end_object();
    void parse_end_array();
    void parse_key( const char *, const char *);
    void parse_string( const char *, const char *);
    void parse_number( const char *, const char *);
    void parse_keyword( const char *, const char *);

  protected:

    struct node {
      uint32_t type_:8;
      uint32_t next_:24;
      union {
        struct { uint32_t h_; uint32_t t_; }; // head/tail of obj/arr
        struct { uint32_t k_; uint32_t v_; }; // key/value pair
        struct { uint32_t p_; uint32_t s_; }; // pos/size offsets
      };
    };

    uint32_t new_node( type_t, uint32_t, uint32_t );
    void add( uint32_t );
    void add_obj( uint32_t );
    void add_arr( uint32_t );

    typedef std::vector<node>     node_vec_t;
    typedef std::vector<uint32_t> stack_t;
    node_vec_t nv_;
    stack_t    st_;
    uint32_t   key_;
    const char*buf_;
  };

  ///////////////////////////////////////////////////////////////////////
  // inline implementation

  inline jtree::type_t jtree::get_type( uint32_t i ) const
  {
    return (type_t)nv_[i].type_;
  }

  inline uint32_t jtree::get_next( uint32_t i ) const
  {
    return nv_[i].next_;
  }

  inline uint32_t jtree::get_first( uint32_t i ) const
  {
    return nv_[i].h_;
  }

  inline uint32_t jtree::get_last( uint32_t i ) const
  {
    return nv_[i].t_;
  }

  inline uint32_t jtree::get_key( uint32_t i ) const
  {
    return nv_[i].k_;
  }

  inline uint32_t jtree::get_val( uint32_t i ) const
  {
    return nv_[i].v_;
  }

  inline void jtree::get_text(
      uint32_t i, const char *&ptr, size_t& sz ) const
  {
    const node& n = nv_[i];
    ptr = &buf_[n.p_];
    sz  = n.s_;
  }

  inline str jtree::get_str( uint32_t i ) const
  {
    const node& n = nv_[i];
    return str( &buf_[n.p_], n.s_ );
  }

  inline uint64_t jtree::get_uint( uint32_t i ) const
  {
    const node& n = nv_[i];
    return str_to_uint( &buf_[n.p_], n.s_ );
  }

  inline int64_t jtree::get_int( uint32_t i ) const
  {
    const node& n = nv_[i];
    return str_to_int( &buf_[n.p_], n.s_ );
  }

  inline bool jtree::get_bool( uint32_t i) const
  {
    const node& n = nv_[i];
    return buf_[n.p_] == 't';
  }

}
