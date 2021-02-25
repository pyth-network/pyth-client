#pragma once

#include <pc/net_socket.hpp>
#include <pc/jwriter.hpp>
#include <pc/jtree.hpp>
#include <pc/key_pair.hpp>
#include <unordered_map>

namespace pc
{

  class rpc_request;

  // solana rpc REST API client
  class rpc_client : public error
  {
  public:

    rpc_client();

    // rpc http connection
    void set_http_conn( net_socket * );
    net_socket *get_http_conn() const;

    // rpc web socket connection
    void set_ws_conn( net_socket * );
    net_socket *get_ws_conn() const;

    // submit rpc request (and bundled callback)
    void send( rpc_request * );

  public:

    // parse json payload and invoke callback
    void parse_response( const char *msg, size_t msg_len );

    // add/remove request from notification map
    void add_notify( rpc_request * );
    void remove_notify( rpc_request * );

  private:

    struct rpc_http : public http_client {
      void parse_content( const char *, size_t ) override;
      rpc_client *cp_;
    };

    struct rpc_ws : public ws_parser {
      void parse_msg( const char *msg, size_t msg_len ) override;
      rpc_client *cp_;
    };

    typedef std::vector<rpc_request*>    request_t;
    typedef std::vector<uint64_t>        id_vec_t;
    typedef std::unordered_map<uint64_t,rpc_request*> sub_map_t;

    void add_request( rpc_request *rptr );

    net_socket *hptr_;
    net_socket *wptr_;
    rpc_http    hp_;    // http parser wrapper
    rpc_ws      wp_;    // websocket parser wrapper
    jtree       jp_;    // json parser
    jwriter     jw_;    // json message builder
    request_t   rv_;    // waiting requests by id
    id_vec_t    reuse_; // reuse id list
    sub_map_t   smap_;  // subscription map
    uint64_t    id_;    // next request id
    char        jb_[1024];
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

    // rpc response callback
    void set_sub( rpc_sub * );
    rpc_sub *get_sub() const;

    // is this message http or websocket bound
    virtual bool get_is_http() const;

    // request builder
    virtual void request( jwriter& ) = 0;

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
    // get account balance, program data and account meta-data
    class get_account_info : public rpc_request
    {
    public:
      // parameters
      void set_account( const pub_key& );

      // results
      uint64_t get_slot() const;
      uint64_t get_lamports() const;
      uint64_t get_rent_epoch() const;
      bool     get_is_executable() const;
      void     get_owner( const char *&, size_t& ) const;
      void     get_data( const char *&, size_t& ) const;

      get_account_info();
      void request( jwriter& ) override;
      void response( const jtree& ) override;

    private:
      pub_key     acc_;
      uint64_t    slot_;
      uint64_t    lamports_;
      uint64_t    rent_epoch_;
      const char *dptr_;
      size_t      dlen_;
      const char *optr_;
      size_t      olen_;
      bool        is_exec_;
    };

    // recent block hash and fee schedule
    class get_recent_block_hash : public rpc_request
    {
    public:
      // results
      uint64_t get_slot() const;
      hash     get_block_hash() const;
      uint64_t get_lamports_per_signature() const;

      get_recent_block_hash();
      void request( jwriter& ) override;
      void response( const jtree& ) override;

    private:
      uint64_t  slot_;
      hash      bhash_;
      uint64_t  fee_per_sig_;
    };

    // signature (transaction) subscription for tx acknowledgement
    class signature_subscribe : public rpc_subscription
    {
    public:
      // parameters
      void set_signature( const signature& );

      void request( jwriter& ) override;
      void response( const jtree& ) override;
      bool notify( const jtree& ) override;

    private:
      signature sig_;
    };

    // transaction to transfer funds between accounts
    class transfer : public rpc_request
    {
    public:
      // parameters
      void set_block_hash( const hash& );
      void set_sender( const key_pair& );
      void set_receiver( const pub_key& );
      void set_lamports( uint64_t funds );

      // results
      signature get_signature() const;
      void enc_signature( std::string& );

      transfer();
      void request( jwriter& ) override;
      void response( const jtree& ) override;

    private:
      hash      bhash_;
      key_pair  snd_;
      pub_key   rcv_;
      uint64_t  lamports_;
      signature sig_;
    };

  }

}
