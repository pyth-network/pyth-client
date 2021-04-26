#pragma once

#include <pc/misc.hpp>

namespace pc
{

  class key_pair;

  class hash
  {
  public:
    static const size_t len = 32;
    hash();
    hash( const hash& );
    hash& operator=( const hash& );
    bool operator==( const hash& )const;
    bool operator!=( const hash& )const;
    void zero();

    // initialize from base58 encoded file or text
    bool init_from_file( const std::string& file );
    bool init_from_text( const std::string& buf );
    bool init_from_text( str buf );
    void init_from_buf( const uint8_t * );

    // encode to text buffer
    int enc_base58( uint8_t *buf, uint32_t buflen ) const;
    int enc_base58( std::string& ) const;
    int dec_base58( const uint8_t *buf, uint32_t buflen );

    // get underlying bytes
    const uint8_t *data() const;

  protected:
    union{ uint64_t i_[4]; uint8_t pk_[len]; };
  };

  // public key
  class pub_key : public hash
  {
  public:
    pub_key();
    pub_key( const pub_key& );
    pub_key( const key_pair& );
    pub_key& operator=( const pub_key& );
  };

  // private/public key pair
  class key_pair
  {
  public:
    static const size_t len = 64;

    void zero();

    // generate new keypair
    void gen();

    // initialize from json-format key file used by solana
    bool init_from_file( const std::string& file );
    bool init_from_json( const char *buf, size_t len );
    bool init_from_json( const std::string& buf );

    // get public key part of pair
    void get_pub_key( pub_key& ) const;

    // get underlying bytes
    const uint8_t *data() const;

  private:
    uint8_t pk_[len];
  };

  // digital signature
  class signature
  {
  public:

    static const size_t len = 64;

    // initialize from raw bytes or bas58 encoded text
    void init_from_buf( const uint8_t * );
    bool init_from_text( const std::string& buf );

    // encode to text buffer
    int enc_base58( uint8_t *buf, uint32_t buflen );
    int enc_base58( std::string& );

    // sign message given key_pair
    bool sign( const uint8_t* msg, uint32_t msg_len,
               const key_pair& );

    // verify message given public key
    bool verify( const uint8_t* msg, uint32_t msg_len,
                 const pub_key& );

    // get underlying bytes
    const uint8_t *data() const;

  private:
    uint8_t sig_[len];
  };

  inline const uint8_t *hash::data() const
  {
    return pk_;
  }

  inline const uint8_t *key_pair::data() const
  {
    return pk_;
  }

  inline const uint8_t *signature::data() const
  {
    return sig_;
  }

}
