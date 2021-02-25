#pragma once

#include <stdint.h>
#include <vector>

namespace pc
{

  // json message constructor
  class jwriter
  {
  public:

    jwriter();
    void attach( char *buf );

    // add or pop new object/array
    void add_obj();
    void add_arr();
    void pop();

    // add key/value pair in object
    void add_key( const char *key, size_t key_len,
                  const char *val, size_t val_len );
    void add_key( const char *key, const char *val, size_t val_len );
    void add_key( const char *key, const char *val );
    void add_key( const char *key, uint64_t val );

    // add array value
    void add_val( const char *val, size_t val_len );
    void add_val( const char *val );
    void add_val( uint64_t  );

  private:

    typedef enum { e_obj = 0, e_arr } type_t;
    typedef std::vector<type_t> type_vec_t;

    void add_key_only( const char *key, size_t key_len );

    char      *buf_;
    size_t     wtr_;
    bool       first_;
    type_vec_t st_;
  };

}
