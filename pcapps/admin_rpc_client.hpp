#pragma once

#include <pc/rpc_client.hpp>

namespace pc {

  namespace rpc {

    // get minimum balance needed to maintain to be exempt rent
    class get_minimum_balance_rent_exemption : public rpc_request
    {
    public:
      get_minimum_balance_rent_exemption();
      void set_size( size_t );
      uint64_t get_lamports() const;
      void request( json_wtr& ) override;
      void response( const jtree&p) override;
    private:
      size_t   sz_;
      uint64_t lamports_;
    };

    // create new account and assign ownership to some (program) key
    class create_account : public rpc_request
    {
    public:
      // parameters
      void set_block_hash( hash * );
      void set_sender( key_pair * );
      void set_account( key_pair * );
      void set_owner( pub_key * );
      void set_lamports( uint64_t funds );
      void set_space( uint64_t num_bytes );

      // results
      signature *get_signature();

      create_account();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      hash     *bhash_;
      key_pair *snd_;
      key_pair *account_;
      pub_key  *owner_;
      uint64_t  lamports_;
      uint64_t  space_;
      signature sig_;
    };

    // signature (transaction) subscription for tx acknowledgement
    class signature_subscribe : public rpc_subscription
    {
    public:
      // parameters
      void set_signature( signature * );
      void set_commitment( commitment );

      // results
      uint64_t get_slot() const;

      signature_subscribe();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;
      bool notify( const jtree& ) override;

    private:
      signature  *sig_;
      commitment  cmt_;
      uint64_t    slot_;
    };

    // initialize mapping account
    class init_mapping : public rpc_request
    {
    public:
      // parameters
      void set_block_hash( hash * );
      void set_publish( key_pair * );
      void set_mapping( key_pair * );
      void set_program( pub_key * );

      // results
      signature *get_signature();

      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      hash     *bhash_;
      key_pair *pkey_;
      key_pair *mkey_;
      pub_key  *gkey_;
      signature sig_;
    };

    // add new mapping account
    class add_mapping : public rpc_request
    {
    public:
      // parameters
      void set_block_hash( hash * );
      void set_publish( key_pair * );
      void set_mapping( key_pair * );
      void set_account( key_pair * );
      void set_program( pub_key * );

      // results
      signature *get_signature();

      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      hash     *bhash_;
      key_pair *pkey_;
      key_pair *mkey_;
      key_pair *akey_;
      pub_key  *gkey_;
      signature sig_;
    };

    // create and add new product account to mapping account
    class add_product : public rpc_request
    {
    public:
      void set_program( pub_key * );
      void set_block_hash( hash * );
      void set_publish( key_pair * );
      void set_account( key_pair * );
      void set_mapping( key_pair * );
      price_type get_price_type() const;

      // results
      signature *get_signature();

      add_product();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      hash      *bhash_;
      key_pair  *pkey_;
      pub_key   *gkey_;
      key_pair  *akey_;
      key_pair  *mkey_;
      signature  sig_;
    };

    // update product attribute data
    class upd_product : public rpc_request
    {
    public:
      void set_program( pub_key * );
      void set_block_hash( hash * );
      void set_publish( key_pair * );
      void set_account( key_pair * );
      void set_attr_dict( attr_dict* );
      price_type get_price_type() const;

      // results
      signature *get_signature();

      upd_product();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      hash      *bhash_;
      key_pair  *pkey_;
      pub_key   *gkey_;
      key_pair  *akey_;
      attr_dict *aptr_;
      signature  sig_;
    };

    // create and add new price account to product account
    class add_price : public rpc_request
    {
    public:
      void set_exponent( int32_t );
      void set_price_type( price_type );
      void set_program( pub_key * );
      void set_block_hash( hash * );
      void set_publish( key_pair * );
      void set_account( key_pair * );
      void set_product( key_pair * );
      price_type get_price_type() const;

      // results
      signature *get_signature();

      add_price();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      int32_t    expo_;
      price_type ptype_;
      hash      *bhash_;
      key_pair  *pkey_;
      pub_key   *gkey_;
      key_pair  *akey_;
      key_pair  *skey_;
      signature  sig_;
    };

    // (re) initialize price account
    class init_price : public rpc_request
    {
    public:
      void set_exponent( int32_t );
      void set_price_type( price_type );
      void set_program( pub_key * );
      void set_block_hash( hash * );
      void set_publish( key_pair * );
      void set_account( key_pair * );
      price_type get_price_type() const;

      // results
      signature *get_signature();

      init_price();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      int32_t    expo_;
      price_type ptype_;
      hash      *bhash_;
      key_pair  *pkey_;
      pub_key   *gkey_;
      key_pair  *akey_;
      signature  sig_;
    };

    // set min publishers on price account
    class set_min_pub_rpc : public rpc_request
    {
    public:
      void set_min_pub( uint8_t );
      void set_program( pub_key * );
      void set_block_hash( hash * );
      void set_publish( key_pair * );
      void set_account( key_pair * );

      // results
      signature *get_signature();

      set_min_pub_rpc();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      uint8_t    min_pub_;
      hash      *bhash_;
      key_pair  *pkey_;
      pub_key   *gkey_;
      key_pair  *akey_;
      signature  sig_;
    };

    // add new price publisher to symbol account
    class add_publisher : public rpc_request
    {
    public:
      void set_publisher( const pub_key& );
      void set_program( pub_key * );
      void set_block_hash( hash * );
      void set_publish( key_pair * );
      void set_account( key_pair * );
      pub_key *get_publisher();

      // results
      signature *get_signature();

      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      pub_key    nkey_;
      hash      *bhash_;
      key_pair  *pkey_;
      pub_key   *gkey_;
      key_pair  *akey_;
      signature  sig_;
    };

    // remove price publisher from symbol account
    class del_publisher : public rpc_request
    {
    public:
      void set_publisher( const pub_key& );
      void set_program( pub_key * );
      void set_block_hash( hash * );
      void set_publish( key_pair * );
      void set_account( key_pair * );
      void set_price_type( price_type );
      pub_key *get_publisher();

      // results
      signature *get_signature();

      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      pub_key    nkey_;
      hash      *bhash_;
      key_pair  *pkey_;
      pub_key   *gkey_;
      key_pair  *akey_;
      signature  sig_;
    };

    // initialize test account
    class init_test : public rpc_request
    {
    public:
      // parameters
      void set_block_hash( hash * );
      void set_publish( key_pair * );
      void set_account( key_pair * );
      void set_program( pub_key * );

      // results
      signature *get_signature();

      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      hash     *bhash_;
      key_pair *pkey_;
      key_pair *akey_;
      pub_key  *gkey_;
      signature sig_;
    };

    // run aggregate compte test
    class upd_test : public rpc_request
    {
    public:
      // parameters
      upd_test();
      void set_block_hash( hash * );
      void set_publish( key_pair * );
      void set_account( key_pair * );
      void set_program( pub_key * );
      void set_expo( int );
      void set_num( uint32_t );
      void set_price( unsigned i, int64_t px, uint64_t conf, int64_t sdiff );

      // results
      signature *get_signature();

      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      hash     *bhash_;
      key_pair *pkey_;
      key_pair *tkey_;
      pub_key  *gkey_;
      signature sig_;
      cmd_upd_test_t upd_[1];
    };

  }

}
