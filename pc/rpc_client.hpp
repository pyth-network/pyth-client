#pragma once

#include <pc/net_socket.hpp>
#include <pc/jwriter.hpp>
#include <pc/jtree.hpp>
#include <pc/key_pair.hpp>

namespace pc
{

  class rpc_request;

  // solana rpc REST API client
  class rpc_client : public http_client,
                     public ws_parser
  {
  public:

    rpc_client();

    // rpc http connection
    void set_http_conn( net_socket * );
    net_socket *get_http_conn() const;

    // rpc web socket connection
    void set_ws_conn( net_socket * );
    net_socket *get_ws_conn() const;

    // submit rpc http request (and bundled callback)
    void send_http( rpc_request * );

    // submit rpc websocket request (and bundled callback)
    void send_ws( rpc_request * );

    // parse json payload and invoke callback
    void parse_content( const char *content, size_t content_len ) override;
    void parse_msg( const char *msg, size_t msg_len ) override;

  private:

    typedef std::vector<rpc_request*> request_t;
    typedef std::vector<uint64_t>     reuse_t;

    void add_request( rpc_request *rptr );
    void parse_response( const char *msg, size_t msg_len );

    net_socket *hptr_;
    net_socket *wptr_;
    jtree       jp_; // json parser
    jwriter     jw_; // json message builder
    request_t   rv_; // waiting requests by id
    reuse_t     rd_; // reuse id list
    uint64_t    id_; // next id
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
  class rpc_request
  {
  public:
    rpc_request();
    virtual ~rpc_request();

    // rpc response callback
    void set_sub( rpc_sub * );
    rpc_sub *get_sub() const;
    template<class T> void on_response( T * );

    // request builder
    virtual void serialize( jwriter& ) = 0;

    // response parsing and callback
    virtual void deserialize( const jtree& ) = 0;

  private:
    rpc_sub *cb_;
  };

  template<class T>
  void rpc_request::on_response( T *req )
  {
    rpc_sub_i<T> *iptr = dynamic_cast<rpc_sub_i<T>*>( req->get_sub() );
    if ( iptr ) {
      iptr->on_response( req );
    }
  }

  /////////////////////////////////////////////////////////////////////////
  // wrappers for various solana rpc requests

  namespace rpc
  {
    // get account balance, program and meta-data
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
      void serialize( jwriter& ) override;
      void deserialize( const jtree& ) override;

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
      void serialize( jwriter& ) override;
      void deserialize( const jtree& ) override;

    private:
      uint64_t  slot_;
      hash      bhash_;
      uint64_t  fee_per_sig_;
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
      void serialize( jwriter& ) override;
      void deserialize( const jtree& ) override;

    private:
      hash      bhash_;
      key_pair  snd_;
      pub_key   rcv_;
      uint64_t  lamports_;
      signature sig_;
    };

  }

}
