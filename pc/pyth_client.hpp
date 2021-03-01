#pragma once

#include <pc/net_socket.hpp>
#include <pc/rpc_client.hpp>
#include <pc/prev_next.hpp>
#include <pc/dbl_list.hpp>

// status bits
#define PC_PYTH_RPC_CONNECTED    (1<<0)
#define PC_PYTH_HAS_BLOCK_HASH   (1<<1)
#define PC_PYTH_HAS_MASTER_CACHE (1<<2)

namespace pc
{

  class pyth_server;

  // pyth-client connection
  class pyth_client : public prev_next<pyth_client>,
                      public net_connect,
                      public ws_parser
  {
  public:
    pyth_client();

    // associated rpc connection
    void set_rpc_client( rpc_client * );

    // associated pyth server
    void set_pyth_server( pyth_server * );

    // http request message parsing
    void parse_content( const char *, size_t );

    // websocket message parsing
    void parse_msg( const char *buf, size_t sz ) override;

    // client disconnected
    void teardown() override;

  private:

    // http-only request parsing
    struct pyth_http : public http_server {
      void parse_content( const char *, size_t ) override;
      pyth_client *ptr_;
    };

    rpc_client  *rptr_;    // rpc client api
    pyth_server *sptr_;    // client collection
    pyth_http    hsvr_;    // http parser
  };

  // pyth client api request
  class pyth_request : public prev_next<pyth_request>,
                       public error,
                       public rpc_sub
  {
  public:

    // pyth gateway server
    void set_pyth_server( pyth_server * );
    pyth_server *get_pyth_server() const;

    // rpc client interface
    void set_rpc_client( rpc_client * );
    rpc_client *get_rpc_client() const;

    // has request finished

    // submit requests as required
    virtual void submit() = 0;

  private:
    pyth_server *svr_;
    rpc_client  *clnt_;
  };

  // pyth-client connection management
  class pyth_server : public error,
                      public net_accept,
                      public rpc_sub,
                      public rpc_sub_i<rpc::get_recent_block_hash>
  {
  public:

    pyth_server();

    // solana rpc http connection
    void set_rpc_host( const std::string& );
    std::string get_rpc_host() const;

    // server listening port
    void set_listen_port( int port );
    int get_listen_port() const;

    // private/public key-pair file
    void set_key_file( const std::string& );
    std::string get_key_file() const;
    key_pair get_key() const;

    // recent block hash stuff
    void set_recent_block_hash_timeout( int64_t );
    int64_t get_recent_block_hash_timeout() const;
    hash get_recent_block_hash() const;

    // submit pyth client api request
    void submit( pyth_request * );

    // check status condition
    bool has_status( int status ) const;

    // schedule client for termination
    void del_client( pyth_client * );

    // initialize server and loop
    bool init();

    // poll for socket updates
    void poll();

    // accept new pyth client apps
    void accept( int fd );

    // shut-down server
    void teardown();

  public:

    // rpc callbacks
    void on_response( rpc::get_recent_block_hash * );

  private:

    typedef dbl_list<pyth_client>  clnt_list_t;
    typedef dbl_list<pyth_request> req_list_t;
    void reconnect_rpc();
    void log_disconnect();
    void teardown_clients();
    void set_status( int );
    void reset_status( int );

    net_loop    nl_;       // epoll loop
    tcp_connect hconn_;    // rpc http connection
    ws_connect  wconn_;    // rpc websocket sonnection
    tcp_listen  lsvr_;     // listening socket
    rpc_client  clnt_;     // rpc api
    clnt_list_t olist_;    // open clients list
    clnt_list_t dlist_;    // to-be-deleted client list
    req_list_t  plist_;    // pending requests
    key_pair    key_;      // private/public key-pair
    std::string rhost_;    // rpc host
    std::string kfile_;    // key-pair file
    int         status_;   // status bitmap
    int64_t     cts_;      // (re)connect timestamp
    int64_t     ctimeout_; // connection timeout
    int64_t     btimeout_; // blockhash request timeout

    // requests
    rpc::get_recent_block_hash bh_[1]; // block hash request
  };

  namespace pyth
  {

    // create new mapping account
    class create_mapping : public pyth_request,
                           public rpc_sub_i<rpc::create_account>,
                           public rpc_sub_i<rpc::signature_subscribe>
    {
    public:

      create_mapping();
      void submit() override;
      void on_response( rpc::create_account * );
      void on_response( rpc::signature_subscribe * );
      rpc::create_account      *get_create_account();
      rpc::signature_subscribe *get_signature_subscribe();

    private:
      typedef enum {
        e_new,
        e_create_sent, e_create_done,
        e_setup_sent, e_setup_done,
        e_error
      } state_t;

      state_t                  st_;
      rpc::create_account      req_[1];
      rpc::signature_subscribe sig_[1];
    };

  };

}
