#pragma once

#include <pc/net_socket.hpp>
#include <pc/rpc_client.hpp>
#include <pc/pyth_request.hpp>
#include <pc/key_store.hpp>
#include <pc/prev_next.hpp>
#include <pc/dbl_list.hpp>
#include <pc/hash_map.hpp>

// status bits
#define PC_PYTH_RPC_CONNECTED    (1<<0)
#define PC_PYTH_HAS_BLOCK_HASH   (1<<1)
#define PC_PYTH_HAS_MAPPING      (1<<2)

namespace pc
{

  class pyth_client;

  // pyth daemon user connection
  class pyth_user : public prev_next<pyth_user>,
                    public net_connect,
                    public ws_parser
  {
  public:
    pyth_user();

    // associated rpc connection
    void set_rpc_client( rpc_client * );

    // associated pyth server
    void set_pyth_client( pyth_client * );

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
      pyth_user *ptr_;
    };

    void parse_request( uint32_t );
    void parse_upd_price(uint32_t,  uint32_t );
    void add_header();
    void add_tail( uint32_t id );
    void add_parse_error();
    void add_invalid_request( uint32_t id = 0 );
    void add_invalid_params( uint32_t id );
    void add_unknown_symbol( uint32_t id );
    void add_error( uint32_t id, int err, str );

    rpc_client  *rptr_;    // rpc client api
    pyth_client *sptr_;    // client collection
    pyth_http    hsvr_;    // http parser
    jtree        jp_;      // json parser
    json_wtr     jw_;      // json writer
  };

  // pyth-client connection management
  class pyth_client : public key_store,
                      public net_accept,
                      public rpc_sub,
                      public rpc_sub_i<rpc::get_recent_block_hash>
  {
  public:

    pyth_client();
    virtual ~pyth_client();

    // solana rpc http connection
    void set_rpc_host( const std::string& );
    std::string get_rpc_host() const;

    // server listening port
    void set_listen_port( int port );
    int get_listen_port() const;

    // rpc client interface
    rpc_client *get_rpc_client();

    // recent block hash stuff
    void set_recent_block_hash_timeout( int64_t );
    int64_t get_recent_block_hash_timeout() const;
    hash *get_recent_block_hash();

    // add new symbol
    void add_symbol( const symbol&, const pub_key& );
    pyth_symbol_price *get_symbol_price( const symbol& );

    // submit pyth client api request
    void submit( pyth_request * );

    // check status condition
    bool has_status( int status ) const;

    // schedule client for termination
    void del_client( pyth_user * );

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
    void set_status( int );

  private:

    struct trait {
      static const size_t hsize_ = 8363UL;
      typedef uint32_t      idx_t;
      typedef symbol        key_t;
      typedef const symbol& keyref_t;
      typedef pyth_symbol_price *val_t;
      struct hash_t {
        idx_t operator() ( keyref_t s ) { return s.hash(); }
      };
    };

    typedef dbl_list<pyth_user>             user_list_t;
    typedef dbl_list<pyth_request>          req_list_t;
    typedef std::vector<pyth_get_mapping*>  map_vec_t;
    typedef std::vector<pyth_symbol_price*> spx_vec_t;
    typedef hash_map<trait>                 sym_map_t;

    void reconnect_rpc();
    void log_disconnect();
    void teardown_clients();
    void reset_status( int );
    void init_mapping();
    void clear_mapping();

    net_loop     nl_;       // epoll loop
    tcp_connect  hconn_;    // rpc http connection
    ws_connect   wconn_;    // rpc websocket sonnection
    tcp_listen   lsvr_;     // listening socket
    rpc_client   clnt_;     // rpc api
    user_list_t  olist_;    // open clients list
    user_list_t  dlist_;    // to-be-deleted client list
    req_list_t   plist_;    // pending requests
    map_vec_t    mvec_;     // mapping account updates
    sym_map_t    smap_;     // symbol to account/pricing cache
    spx_vec_t    svec_;     // symbol price subscriber/publishers
    std::string  rhost_;    // rpc host
    int          status_;   // status bitmap
    int64_t      cts_;      // (re)connect timestamp
    int64_t      ctimeout_; // connection timeout
    int64_t      btimeout_; // blockhash request timeout

    // requests
    rpc::get_recent_block_hash bh_[1]; // block hash request
  };

}
