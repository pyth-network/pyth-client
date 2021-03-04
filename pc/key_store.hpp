#pragma once

#include <pc/key_pair.hpp>
#include <pc/error.hpp>
#include <vector>

namespace pc
{

  class key_store : public error
  {
  public:

    key_store();

    // initialize
    bool init();

    // directory where to store account keys
    void set_dir( const std::string& dir_name );
    std::string get_dir() const;

    // file names
    std::string get_publish_key_pair_file() const;
    std::string get_mapping_key_pair_file() const;
    std::string get_mapping_pub_key_file() const;
    std::string get_program_pub_key_file() const;

    // primary publishing and funding key
    key_pair *create_publish_key_pair();
    key_pair *get_publish_key_pair();

    // get mapping key_pair or public key
    key_pair *create_mapping_key_pair();
    key_pair *get_mapping_key_pair();
    pub_key  *get_mapping_pub_key();

    // get program_id public key
    pub_key  *get_program_pub_key();

    // create new mapping or symbol account
    bool create_account_key_pair( key_pair& );
    bool get_account_key_pair( const pub_key&, key_pair& );

  private:

    bool write_key_file( const std::string& key_file,
                         const key_pair& pk );

    bool        has_pkey_;
    bool        has_mkey_;
    bool        has_mpub_;
    bool        has_gkey_;
    key_pair    pkey_; // primary publishing and funding key
    key_pair    mkey_; // mapping account key
    pub_key     mpub_; // mapping account public key
    pub_key     gpub_; // program id
    std::string dir_;  // key store directory
  };

}
