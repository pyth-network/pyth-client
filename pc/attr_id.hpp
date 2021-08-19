#pragma once

#include <pc/hash_map.hpp>
#include <pc/net_socket.hpp>
#include <pc/misc.hpp>
#include <pc/jtree.hpp>
#include <oracle/oracle.h>

namespace pc
{


  // product reference attribute id
  class attr_id
  {
  public:

    attr_id();
    attr_id( uint32_t );
    attr_id( str );
    bool     is_valid() const;
    str      get_str() const;
    uint32_t get_id() const;
    static attr_id add( str );

  private:
    uint32_t  id_;
  };

  // dictionary of attribute values corresponding to attribute ids
  class attr_dict
  {
  public:

    // get string value corresponding to attribute id
    bool get_attr( attr_id, str& ) const;

    // number of attributes in dictionary
    unsigned get_num_attr() const;

    // for iterating through attributes - for example:
    // str vstr;
    // attr_dict d;
    // for(attr_id id; d.get_next_attr( id, vstr ); ) {
    //   std::string key( id.get_str() ), val( vstr );
    //   std::cout << "attr=" << key << ",val=" << val << std::endl;
    // }
    bool get_next_attr( attr_id&, str& ) const;

  public:

    // initialize from on-chain product account data
    bool init_from_account( pc_prod_t * );

    // initialize from json parse tree
    bool init_from_json( jtree&, uint32_t tok = 1 );

    // serialize in account format
    void write_account( net_wtr& );

    // serialize to json
    void write_json( json_wtr& ) const;

  private:

    struct pos { pos(); uint16_t idx_, len_; };

    typedef std::vector<char> abuf_t;
    typedef std::vector<pos>  attr_t;

    void add_ref( attr_id aid, str val );
    void clear();

    abuf_t   abuf_;
    attr_t   avec_;
    unsigned num_;
  };

  // manager unique set of attribute ids
  class attr_id_set
  {
  public:

    // get or add attribute id
    attr_id add_attr_id( str );

    // get attribute by string
    attr_id get_attr_id( str );

    // get string from attribute id
    str get_str( attr_id );

    // is a valid attribute id
    bool is_valid( attr_id );

    // number of attribute ids
    unsigned get_num_attr_id() const;

    // singleton instance
    static attr_id_set& inst();

  private:

    attr_id_set();

    class attr_wtr : public net_wtr {
    public:
      str add_attr( str );
    };

    struct trait_str {
      static const size_t hsize_ = 307UL;
      typedef uint32_t   idx_t;
      typedef str        key_t;
      typedef str        keyref_t;
      typedef attr_id    val_t;
      struct hash_t {
        idx_t operator() ( keyref_t s ) {
          if ( s.len_ >= 8 ) return ((uint64_t*)s.str_)[0];
          if ( s.len_ >= 4 ) return ((uint32_t*)s.str_)[0];
          if ( s.len_ >= 2 ) return ((uint16_t*)s.str_)[0];
          return s.len_ ? (idx_t)s.str_[0] : 0;
        }
      };
    };

    typedef hash_map<trait_str> attr_map_t;
    typedef std::vector<str>    attr_vec_t;
    attr_map_t amap_;
    attr_vec_t avec_;
    attr_wtr   abuf_;
    uint32_t   aid_;
  };

  inline unsigned attr_id_set::get_num_attr_id() const
  {
    return aid_;
  }

  inline bool attr_id_set::is_valid( attr_id aid )
  {
    return aid.get_id() != 0 && aid.get_id() < avec_.size();
  }

  inline attr_id::attr_id() : id_( 0 ) {
  }

  inline attr_id::attr_id( uint32_t id ) : id_( id ) {
  }

  inline attr_id::attr_id( str v ) {
    *this = attr_id_set::inst().get_attr_id( v );
  }

  inline bool attr_id::is_valid() const {
    return attr_id_set::inst().is_valid( *this );
  }

  inline str attr_id::get_str() const {
    return attr_id_set::inst().get_str( *this );
  }

  inline uint32_t attr_id::get_id() const {
    return id_;
  }

  inline attr_id attr_id::add( str val ) {
    return attr_id_set::inst().add_attr_id( val );
  }

  inline attr_id_set& attr_id_set::inst() {
    static attr_id_set aset;
    return aset;
  }

}
