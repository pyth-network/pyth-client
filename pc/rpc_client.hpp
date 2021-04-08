#pragma once

#include <pc/net_socket.hpp>
#include <pc/jtree.hpp>
#include <pc/key_pair.hpp>
#include <oracle/oracle.h>
#include <pc/hash_map.hpp>

#define PC_RPC_ERROR_BLOCK_CLEANED_UP          -32001
#define PC_RPC_ERROR_SEND_TX_PREFLIGHT_FAIL    -32002
#define PC_RPC_ERROR_TX_SIG_VERIFY_FAILURE     -32003
#define PC_RPC_ERROR_BLOCK_NOT_AVAILABLE       -32004
#define PC_RPC_ERROR_NODE_UNHEALTHY            -32005
#define PC_RPC_ERROR_TX_PRECOMPILE_VERIFY_FAIL -32006
#define PC_RPC_ERROR_SLOT_SKIPPED              -32007
#define PC_RPC_ERROR_NO_SNAPSHOT               -32008
#define PC_RPC_ERROR_LONG_TERM_SLOT_SKIPPED    -32009

namespace pc
{

  // type of "price" calculation represented by account
  enum class price_type
  {
    e_unknown = PC_PTYPE_UNKNOWN,
    e_price   = PC_PTYPE_PRICE,
    e_twap    = PC_PTYPE_TWAP,

    e_last_price_type
  };

  price_type str_to_price_type( str );
  str price_type_to_str( price_type );

  // current symbol trading status
  enum class symbol_status
  {
    e_unknown = PC_STATUS_UNKNOWN,
    e_trading = PC_STATUS_TRADING,
    e_halted  = PC_STATUS_HALTED,

    e_last_symbol_status
  };

  symbol_status str_to_symbol_status( str );
  str symbol_status_to_str( symbol_status );

  // commitment status of account on blockchain
  enum commitment
  {
    e_unknown = 0,
    e_processed,
    e_confirmed,
    e_finalized,
    e_last_commitment
  };

  str commitment_to_str( commitment );
  commitment str_to_commitment( str );

  class rpc_request;

  // solana rpc REST API client
  class rpc_client : public error
  {
  public:

    rpc_client();

    // rpc http connection
    void set_http_conn( net_connect * );
    net_connect *get_http_conn() const;

    // rpc web socket connection
    void set_ws_conn( net_connect * );
    net_connect *get_ws_conn() const;

    // submit rpc request (and bundled callback)
    void send( rpc_request * );

  public:

    // parse json payload and invoke callback
    void parse_response( const char *msg, size_t msg_len );

    // add/remove request from notification map
    void add_notify( rpc_request * );
    void remove_notify( rpc_request * );

    // decode into buffer and return pointer
    template<class T>
    void get_data( const char *dptr, size_t dlen, T *&ptr );

    // reset state
    void reset();

  private:

    struct rpc_http : public http_client {
      void parse_content( const char *, size_t ) override;
      rpc_client *cp_;
    };

    struct rpc_ws : public ws_parser {
      void parse_msg( const char *msg, size_t msg_len ) override;
      rpc_client *cp_;
    };

    struct trait {
      static const size_t hsize_ = 8363UL;
      typedef uint32_t     idx_t;
      typedef uint64_t     key_t;
      typedef uint64_t     keyref_t;
      typedef rpc_request *val_t;
      struct hash_t {
        idx_t operator() ( keyref_t id ) { return (idx_t)id; }
      };
    };

    typedef std::vector<rpc_request*> request_t;
    typedef std::vector<uint64_t>     id_vec_t;
    typedef std::vector<char>         acc_buf_t;
    typedef hash_map<trait>           sub_map_t;

    net_connect *hptr_;
    net_connect *wptr_;
    rpc_http     hp_;    // http parser wrapper
    rpc_ws       wp_;    // websocket parser wrapper
    jtree        jp_;    // json parser
    request_t    rv_;    // waiting requests by id
    id_vec_t     reuse_; // reuse id list
    sub_map_t    smap_;  // subscription map
    acc_buf_t    abuf_;  // account decode buffer
    uint64_t     id_;    // next request id
  };

  // rpc response or subscrption callback
  class rpc_sub
  {
  public:
    virtual ~rpc_sub();
  };

  // rpc subscription callback for request type T
  template<class T>
  class rpc_sub_i
  {
  public:
    virtual ~rpc_sub_i() {}
    virtual void on_response( T * ) = 0;
  };

  // base-class rpc request message
  class rpc_request : public error
  {
  public:
    rpc_request();
    virtual ~rpc_request();

    // corresponding rpc_client
    void set_rpc_client( rpc_client * );
    rpc_client *get_rpc_client() const;

    // request or subscription id
    void set_id( uint64_t );
    uint64_t get_id() const;

    // error code
    void set_err_code( int );
    int get_err_code() const;

    // time sent
    void set_sent_time( int64_t );
    int64_t get_sent_time() const;

    // time received reply
    void set_recv_time( int64_t );
    int64_t get_recv_time() const;

    // have we received a reply
    bool get_is_recv() const;

    // rpc response callback
    void set_sub( rpc_sub * );
    rpc_sub *get_sub() const;

    // is this message http or websocket bound
    virtual bool get_is_http() const;

    // request builder
    virtual void request( json_wtr& ) = 0;

    // response parsing and callback
    virtual void response( const jtree& ) = 0;

    // notification subscription update
    virtual bool notify( const jtree& );

  protected:

    template<class T> void on_response( T * );
    template<class T> bool on_error( const jtree&, T * );

  private:
    rpc_sub    *cb_;
    rpc_client *cp_;
    uint64_t    id_;
    int         ec_;
    int64_t     sent_ts_;
    int64_t     recv_ts_;
  };

  class rpc_subscription : public rpc_request
  {
  public:
    // subscriptions are only available on websockets
    virtual bool get_is_http() const;

    // add/remove this request to notification list
    void add_notify( const jtree& );
    void remove_notify();
  };

  /////////////////////////////////////////////////////////////////////////
  // wrappers for various solana rpc requests

  template<class T>
  void rpc_client::get_data( const char *dptr, size_t dlen, T *&ptr )
  {
    size_t tlen = enc_base64_len( sizeof( T ) );
    abuf_.resize( std::max( dlen, tlen ) );
    dec_base64( (const uint8_t*)dptr, dlen, (uint8_t*)&abuf_[0] );
    ptr = (T*)&abuf_[0];
  }

  namespace rpc
  {
    // get account balance, program data and account meta-data
    class get_account_info : public rpc_request
    {
    public:
      // parameters
      void set_account( pub_key * );
      void set_commitment( commitment );

      // results
      uint64_t get_slot() const;
      uint64_t get_lamports() const;
      uint64_t get_rent_epoch() const;
      bool     get_is_executable() const;
      void     get_owner( const char *&, size_t& ) const;
      template<class T> void get_data( T *& ) const;

      get_account_info();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      pub_key    *acc_;
      uint64_t    slot_;
      uint64_t    lamports_;
      uint64_t    rent_epoch_;
      const char *dptr_;
      size_t      dlen_;
      const char *optr_;
      size_t      olen_;
      bool        is_exec_;
      commitment  cmt_;
    };

    template<class T> void get_account_info::get_data( T *&res ) const
    {
      get_rpc_client()->get_data( dptr_, dlen_, res );
    }

    // recent block hash and fee schedule
    class get_recent_block_hash : public rpc_request
    {
    public:
      // results
      uint64_t get_slot() const;
      hash    *get_block_hash();
      uint64_t get_lamports_per_signature() const;

      get_recent_block_hash();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      uint64_t  slot_;
      hash      bhash_;
      uint64_t  fee_per_sig_;
    };

    // get validator node health
    class get_health : public rpc_request
    {
    public:
      void request( json_wtr& ) override;
      void response( const jtree& ) override;
    };

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

    // find out when slots update
    class slot_subscribe : public rpc_subscription
    {
    public:
      uint64_t get_slot() const;
      void request( json_wtr& ) override;
      void response( const jtree& ) override;
      bool notify( const jtree& ) override;
    private:
      uint64_t slot_;
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

    // account data subscription
    class account_subscribe : public rpc_subscription
    {
    public:
      // parameters
      void set_account( pub_key * );
      void set_commitment( commitment );

      // results
      uint64_t get_slot() const;
      uint64_t get_lamports() const;
      template<class T> void get_data( T *& ) const;

      account_subscribe();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;
      bool notify( const jtree& ) override;

    private:
      pub_key    *acc_;
      uint64_t    slot_;
      uint64_t    lamports_;
      size_t      dlen_;
      const char *dptr_;
      commitment  cmt_;
    };

    template<class T> void account_subscribe::get_data( T *&res ) const
    {
      get_rpc_client()->get_data( dptr_, dlen_, res );
    }

    // transaction to transfer funds between accounts
    class transfer : public rpc_request
    {
    public:
      // parameters
      void set_block_hash( hash * );
      void set_sender( key_pair * );
      void set_receiver( pub_key * );
      void set_lamports( uint64_t funds );

      // results
      signature *get_signature();
      void enc_signature( std::string& );

      transfer();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      hash     *bhash_;
      key_pair *snd_;
      pub_key  *rcv_;
      uint64_t  lamports_;
      signature sig_;
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

    // add new symbol to mapping account
    class add_symbol: public rpc_request
    {
    public:
      void set_symbol( const symbol& );
      void set_exponent( int32_t );
      void set_price_type( price_type );
      void set_program( pub_key * );
      void set_block_hash( hash * );
      void set_publish( key_pair * );
      void set_account( key_pair * );
      void set_mapping( key_pair * );
      symbol    *get_symbol();
      price_type get_price_type() const;

      // results
      signature *get_signature();

      add_symbol();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      unsigned   num_;
      int32_t    expo_;
      price_type ptype_;
      symbol     sym_;
      hash      *bhash_;
      key_pair  *pkey_;
      pub_key   *gkey_;
      key_pair  *akey_;
      key_pair  *mkey_;
      signature  sig_;
    };

    // add new price publisher to symbol account
    class add_publisher : public rpc_request
    {
    public:
      void set_symbol( const symbol& );
      void set_publisher( const pub_key& );
      void set_program( pub_key * );
      void set_block_hash( hash * );
      void set_publish( key_pair * );
      void set_account( key_pair * );
      void set_price_type( price_type );
      symbol  *get_symbol();
      pub_key *get_publisher();
      price_type get_price_type() const;

      // results
      signature *get_signature();

      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      symbol     sym_;
      pub_key    nkey_;
      hash      *bhash_;
      key_pair  *pkey_;
      pub_key   *gkey_;
      key_pair  *akey_;
      price_type ptype_;
      signature  sig_;
    };

    // remove price publisher from symbol account
    class del_publisher : public rpc_request
    {
    public:
      void set_symbol( const symbol& );
      void set_publisher( const pub_key& );
      void set_program( pub_key * );
      void set_block_hash( hash * );
      void set_publish( key_pair * );
      void set_account( key_pair * );
      void set_price_type( price_type );
      symbol  *get_symbol();
      pub_key *get_publisher();
      price_type get_price_type() const;

      // results
      signature *get_signature();

      void request( json_wtr& ) override;
      void response( const jtree& ) override;

    private:
      symbol     sym_;
      pub_key    nkey_;
      hash      *bhash_;
      key_pair  *pkey_;
      pub_key   *gkey_;
      key_pair  *akey_;
      price_type ptype_;
      signature  sig_;
    };


    // set new component price
    class upd_price : public rpc_request
    {
    public:
      upd_price();
      void set_symbol( symbol * );
      void set_symbol_status( symbol_status );
      void set_price_type( price_type );
      void set_publish( key_pair * );
      void set_account( pub_key * );
      void set_program( pub_key * );
      void set_block_hash( hash * );
      void set_price( int64_t px, uint64_t conf, symbol_status );

      // results
      signature *get_signature();

      void request( json_wtr& ) override;
      void response( const jtree& ) override;
    private:
      symbol       *sym_;
      hash         *bhash_;
      key_pair     *pkey_;
      pub_key      *gkey_;
      pub_key      *akey_;
      int64_t       price_;
      uint64_t      conf_;
      uint64_t      nonce_;
      price_type    pt_;
      symbol_status st_;
      signature     sig_;
    };

  }

}
