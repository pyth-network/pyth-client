#pragma once

#include <pc/net_socket.hpp>
#include <pc/jtree.hpp>

namespace pc
{

  class rpc_request;

  // solana rpc REST API client
  class rpc_client : public http_client
  {
  public:

    rpc_client();

    // submit request (and bundled callback)
    void send( rpc_request * );

    // parse json payload and invoke callback
    void parse_content( const char *content, size_t content_len ) override;

  private:
    typedef std::vector<rpc_request*> request_t;
    typedef std::vector<uint64_t>     reuse_t;
    jtree     jp_; // json parser
    request_t rv_; // waiting requests by id
    reuse_t   rd_; // reuse id list
    uint64_t  id_; // next id
  };

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
    void set_rpc_sub( rpc_sub * );
    rpc_sub *get_rpc_sub() const;
    template<class T> void on_response( T * );

    // request builder
//    virtual void add( jwriter& ) = 0;

    // response parsing and callback
    virtual void deserialize( const jtree& ) = 0;

  private:
    rpc_sub *cb_;
  };

  template<class T>
  void rpc_request::on_response( T *req )
  {
    rpc_sub_i<T> *iptr = dynamic_cast<rpc_sub_i<T>*>( req->get_cb() );
    if ( iptr ) {
      iptr->on_response( req );
    }
  }

  namespace rpc
  {
    class get_account_info : public rpc_request
    {
    public:
      void deserialize( const jtree& ) override;
    };
  }

}
