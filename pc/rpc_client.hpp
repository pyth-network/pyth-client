#pragma once

#include <pc/net_socket.hpp>
#include <pc/jtree.hpp>
#include <pc/key_pair.hpp>
#include <pc/attr_id.hpp>
#include <oracle/oracle.h>
#include <pc/hash_map.hpp>

#include <unordered_map>

#define PC_RPC_ERROR_BLOCK_CLEANED_UP          -32001
#define PC_RPC_ERROR_SEND_TX_PREFLIGHT_FAIL    -32002
#define PC_RPC_ERROR_TX_SIG_VERIFY_FAILURE     -32003
#define PC_RPC_ERROR_BLOCK_NOT_AVAILABLE       -32004
#define PC_RPC_ERROR_NODE_UNHEALTHY            -32005
#define PC_RPC_ERROR_TX_PRECOMPILE_VERIFY_FAIL -32006
#define PC_RPC_ERROR_SLOT_SKIPPED              -32007
#define PC_RPC_ERROR_NO_SNAPSHOT               -32008
#define PC_RPC_ERROR_LONG_TERM_SLOT_SKIPPED    -32009

#define PC_TPU_PROTO_ID 0xb1ab

namespace pc
{
  class bincode;
  class rpc_request;

  // type of "price" calculation represented by account
  enum class price_type
  {
    e_unknown = PC_PTYPE_UNKNOWN,
    e_price   = PC_PTYPE_PRICE,

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

  namespace rpc
  {
    class upd_price;
  }

  // solana rpc REST API client
  class rpc_client : public error
  {
  public:

    rpc_client();
    ~rpc_client();

    // rpc http connection
    void set_http_conn( tcp_connect * );
    tcp_connect *get_http_conn() const;

    // rpc web socket connection
    void set_ws_conn( net_connect * );
    net_connect *get_ws_conn() const;

    // submit rpc request (and bundled callback)
    void send( rpc_request * );
    void send( rpc::upd_price *[], unsigned n, unsigned cu_units, unsigned cu_price );

  public:

    // parse json payload and invoke callback
    void parse_response( const char *msg, size_t msg_len );

    // add/remove request from notification map
    void add_notify( rpc_request * );
    void remove_notify( rpc_request * );

    // decode into internal buffer and return pointer
    size_t get_data_ref(
        const char *dptr, size_t dlen, size_t tlen, char*&ptr);
    // decode into provided buffer
    size_t get_data_val(
        const char *dptr, size_t dlen, size_t tlen, char*ptr);

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

    typedef std::unordered_multimap< uint64_t, rpc_request* > request_t;
    typedef std::vector<uint64_t>     id_vec_t;
    typedef std::vector<char>         acc_buf_t;
    typedef hash_map<trait>           sub_map_t;

    tcp_connect *hptr_;
    net_connect *wptr_;
    rpc_http     hp_;    // http parser wrapper
    rpc_ws       wp_;    // websocket parser wrapper
    jtree        jp_;    // json parser
    request_t    rv_;    // waiting requests by id
    id_vec_t     reuse_; // reuse id list
    sub_map_t    smap_;  // subscription map
    acc_buf_t    abuf_;  // account decode buffer
    acc_buf_t    zbuf_;  // account decompress buffer
    uint64_t     id_;    // next request id
    void        *cxt_;
  };

  // rpc response or subscrption callback
  class rpc_sub
  {
  public:
    virtual ~rpc_sub() {}
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

  struct tx_hdr
  {
    uint16_t proto_id_;
    uint16_t size_;
  };

  // transaction builder
  class tx_request : public error
  {
  public:
    virtual ~tx_request() {}
    virtual void build( net_wtr& ) = 0;
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

  namespace rpc
  {
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

    // get current slot
    class get_slot : public rpc_request
    {
    public:
      get_slot( commitment = e_finalized );
      uint64_t get_current_slot() const;
      void request( json_wtr& ) override;
      void response( const jtree& ) override;
    private:
      commitment const cmt_; // param
      uint64_t cslot_; // result
    };

    // base class for account updates
    class account_update : public rpc_subscription
    {
    public:
      account_update();

      // parameters / results
      pub_key const* get_account() const;
      void set_account( pub_key * );
      void set_commitment( commitment );

      // results
      uint64_t get_slot() const;
      uint64_t get_lamports() const;
      template<class T>
      size_t get_data_ref( T *&, size_t srclen=sizeof(T) ) const;
      template<class T>
      size_t get_data_val( T *, size_t srclen=sizeof(T) ) const;

    protected:
      pub_key     acc_;
      commitment  cmt_;
      uint64_t    slot_;
      uint64_t    lamports_;
      size_t      dlen_;
      const char *dptr_;
    };

    template<class T>
    size_t account_update::get_data_ref( T *&res, size_t tlen ) const
    {
      char *ptr;
      size_t len = get_rpc_client()->get_data_ref( dptr_, dlen_, tlen, ptr );
      res = (T*)ptr;
      return len;
    }

    template<class T>
    size_t account_update::get_data_val( T *res, size_t tlen ) const
    {
      char *ptr = (char*)res;
      size_t len = get_rpc_client()->get_data_val( dptr_, dlen_, tlen, ptr );
      return len;
    }

    // get account balance, program data and account meta-data
    class get_account_info : public account_update
    {
    public:
      // results
      uint64_t get_rent_epoch() const;
      bool     get_is_executable() const;
      void     get_owner( const char *&, size_t& ) const;

      get_account_info();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;

      bool get_is_http() const override;

    private:
      uint64_t    rent_epoch_;
      const char *optr_;
      size_t      olen_;
      bool        is_exec_;
    };

    // account data subscription
    class account_subscribe : public account_update
    {
    public:
      account_subscribe();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;
      bool notify( const jtree& ) override;
    };

    // program subscription
    class program_subscribe : public account_update
    {
    public:
      // parameters
      void set_program( pub_key * );

      program_subscribe();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;
      bool notify( const jtree& ) override;

    private:
      pub_key    *pgm_;
    };

    class get_program_accounts : public account_update
    {
    public:
      // parameters
      void set_program( pub_key * );
      void set_account_type( uint32_t );

      get_program_accounts();
      void request( json_wtr& ) override;
      void response( const jtree& ) override;

      bool get_is_http() const override;

    private:
      pub_key    *pgm_;
      uint32_t    acct_type_;
    };

    // set new component price
    class upd_price : public tx_request, public rpc_request
    {
    public:
      // parameters
      void set_symbol_status( symbol_status );
      void set_publish( key_pair * );
      void set_pubcache( key_cache * );
      void set_account( pub_key * );
      void set_program( pub_key * );
      void set_block_hash( hash * );
      void set_price( int64_t px, uint64_t conf, symbol_status,
                      bool is_aggregate );
      void set_slot( uint64_t );

      uint64_t get_slot();

      // results
      signature *get_signature();
      str        get_ack_signature() const;

      upd_price();
      void build( net_wtr& ) override;
      void request( json_wtr& ) override;
      void response( const jtree& ) override;

      static bool build( net_wtr&, upd_price*[], unsigned n );
      static bool request( json_wtr&, upd_price*[], const unsigned n );

      static bool build( net_wtr&, upd_price*[], unsigned n, unsigned cu_units, unsigned cu_price  );
      static bool request( json_wtr&, upd_price*[], const unsigned n, unsigned cu_units, unsigned cu_price );

    private:
      static bool build_tx( bincode&, upd_price*[], unsigned n, unsigned cu_units, unsigned cu_price );

      hash         *bhash_;
      key_pair     *pkey_;
      key_cache    *ckey_;
      pub_key      *gkey_;
      pub_key      *akey_;
      signature     sig_;
      int64_t       price_;
      uint64_t      conf_;
      uint64_t      pub_slot_;
      command_t     cmd_;
      symbol_status st_;
      str           ack_sig_;
    };

  }

}
