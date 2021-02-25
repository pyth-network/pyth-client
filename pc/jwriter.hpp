#pragma once

#include <stdint.h>
#include <vector>
#include <pc/key_pair.hpp>

namespace pc
{

  // json message constructor
  class jwriter
  {
  public:

    typedef enum { e_obj = 0, e_arr } type_t;

    jwriter();
    jwriter( char * );
    void attach( char *buf );
    char *get_buf() const;
    char *get_wtr() const;
    size_t size() const;

    // add or pop new object/array
    void pop();

    // add key/value pair in object
    void add_key( const char *key, size_t key_len,
                  const char *val, size_t val_len );
    void add_key( const char *key, const char *val, size_t val_len );
    void add_key( const char *key, const char *val );
    void add_key( const char *key, uint64_t val );
    void add_key( const char *key, type_t );
    void add_key( const char *key, const hash& );

    // add array value
    void add_val( const char *val, size_t val_len );
    void add_val( const char *val );
    void add_val( uint64_t  );
    void add_val( type_t  );
    void add_val( const hash& );
    void add_val( const signature& );
    void add_val_enc_base58( const uint8_t *val, size_t val_len );
    void add_val_enc_base64( const uint8_t *val, size_t val_len );

  private:

    typedef std::vector<type_t> type_vec_t;

    void add_key_only( const char *key, size_t key_len );
    void add_obj();
    void add_arr();
    void add_first();

    char      *buf_;
    size_t     wtr_;
    bool       first_;
    type_vec_t st_;
  };

  inline char *jwriter::get_buf() const
  {
    return buf_;
  }

  inline char *jwriter::get_wtr() const
  {
    return &buf_[wtr_];
  }

  inline size_t jwriter::size() const
  {
    return wtr_;
  }

}
