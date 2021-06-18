#pragma once

#include <pc/net_socket.hpp>
#include <pc/rpc_client.hpp>
#include <pc/request.hpp>
#include <pc/user.hpp>
#include <pc/key_store.hpp>
#include <pc/dbl_list.hpp>
#include <pc/hash_map.hpp>
#include <pc/capture.hpp>

// status bits
#define PC_PYTH_RPC_CONNECTED    (1<<0)
#define PC_PYTH_HAS_BLOCK_HASH   (1<<1)
#define PC_PYTH_HAS_MAPPING      (1<<2)

namespace pc
{
  class manager;

  // manager event notification events
  class manager_sub
  {
  public:
    virtual ~manager_sub();

    // on connection to (but not initialization) solana validator
    virtual void on_connect( manager * );

    // on disconnect from solana validator
    virtual void on_disconnect( manager * );

    // on connection to transaction proxy server
    virtual void on_tx_connect( manager * );

    // on disconnect from transaction proxy server
    virtual void on_tx_disconnect( manager * );

    // on completion of (re)bootstrap of accounts following (re)connect
    virtual void on_init( manager * );

    // on addition of new symbols
    virtual void on_add_symbol( manager *, price * );
  };

  // pyth-client connection management and event loop
  class manager : public key_store,
                  public net_accept,
                  public tx_sub,
                  public rpc_sub,
                  public rpc_sub_i<rpc::slot_subscribe>,
                  public rpc_sub_i<rpc::get_recent_block_hash>,
                  public rpc_sub_i<rpc::program_subscribe>
  {
  public:

    manager();
    virtual ~manager();

    // solana rpc http connection
    void set_rpc_host( const std::string& );
    std::string get_rpc_host() const;

    // pyth transaction proxy host
    void set_tx_host( const std::string& );
    std::string get_tx_host() const;

    // turn on/off tx proxy mode
    void set_do_tx( bool );
    bool get_do_tx() const;

    // server listening port
    void set_listen_port( int port );
    int get_listen_port() const;

    // account retrieval/subscription commitment level (default confirmed)
    void set_commitment( commitment );
    commitment get_commitment() const;

    // content directory (for http content requests if running as server)
    void set_content_dir( const std::string& );
    std::string get_content_dir() const;

    // capture flag (off by default)
    void set_do_capture( bool );
    bool get_do_capture() const;

    // price capture file
    void set_capture_file( const std::string& cap_file );
    std::string get_capture_file() const;

    // override default publish interval (in milliseconds)
    void set_publish_interval( int64_t mill_secs );
    int64_t get_publish_interval() const;

    // event subscription callback
    void set_manager_sub( manager_sub * );
    manager_sub *get_manager_sub() const;

    // current time in manager in nanoseconds from epoch
    int64_t get_curr_time() const;

    // rpc client interface
    rpc_client *get_rpc_client();

    // recent block hash stuff
    hash *get_recent_block_hash();

    // get most recently processed slot
    uint64_t get_slot() const;

    // add and subscribe to new mapping account
    void add_mapping( const pub_key& );

    // add new product from mapping account
    void add_product( const pub_key& );

    // add new price account in product
    void add_price( const pub_key&, product * );

    // iterate through products
    unsigned get_num_product() const;
    product *get_product( unsigned i ) const;
    product *get_product( const pub_key& );
    price   *get_price( const pub_key& );

    // submit pyth client api request
    void submit( request * );
    void submit( tx_request * );

    // check status condition
    bool has_status( int status ) const;

    // schedule client for termination
    void del_user( user * );

    // initialize server and loop
    bool init();

    // poll server until fully initialized or in error
    bool bootstrap();

    // poll for socket updates
    void poll( bool do_wait = true);

    // accept new pyth client apps
    void accept( int fd );

    // shut-down server
    void teardown();

  public:

    // mapping subscription count tracking
    void add_map_sub();
    void del_map_sub();
    void schedule( price_sched* );
    void write( pc_pub_key_t *, pc_acc_t *ptr );

    // tx_sub callbacks
    void on_connect() override;
    void on_disconnect() override;
    bool get_is_tx_connect() const;
    bool get_is_tx_send() const;

    // rpc callbacks
    void on_response( rpc::slot_subscribe * ) override;
    void on_response( rpc::get_recent_block_hash * ) override;
    void on_response( rpc::program_subscribe * ) override;
    void set_status( int );
    get_mapping *get_last_mapping() const;

  private:

    struct trait_account {
      static const size_t hsize_ = 8363UL;
      typedef uint32_t        idx_t;
      typedef pub_key         key_t;
      typedef const pub_key&  keyref_t;
      typedef request        *val_t;
      struct hash_t {
        idx_t operator() ( keyref_t a ) {
          uint64_t *i = (uint64_t*)a.data();
          return *i;
        }
      };
    };

    struct tx_parser : public net_parser
    {
      bool parse( const char *buf, size_t sz, size_t& len ) override;
      manager *mgr_;
    };

    typedef dbl_list<user>            user_list_t;
    typedef dbl_list<request>         req_list_t;
    typedef std::vector<get_mapping*> map_vec_t;
    typedef std::vector<product*>     spx_vec_t;
    typedef std::vector<price_sched*> kpx_vec_t;
    typedef hash_map<trait_account>   acc_map_t;

    void reconnect_rpc();
    void log_disconnect();
    void teardown_users();
    void poll_schedule();
    void reset_status( int );

    net_loop     nl_;       // epoll loop
    tcp_connect  hconn_;    // rpc http connection
    ws_connect   wconn_;    // rpc websocket sonnection
    tcp_listen   lsvr_;     // listening socket
    rpc_client   clnt_;     // rpc api
    tx_connect   tconn_;    // tx proxy connection
    user_list_t  olist_;    // open users list
    user_list_t  dlist_;    // to-be-deleted users list
    req_list_t   plist_;    // pending requests
    map_vec_t    mvec_;     // mapping account updates
    acc_map_t    amap_;     // account to symbol pricing info
    spx_vec_t    svec_;     // symbol price subscriber/publishers
    std::string  thost_;    // tx proxy host
    std::string  rhost_;    // rpc host
    std::string  cdir_;     // content directory
    manager_sub *sub_;      // subscription callback
    int          status_;   // status bitmap
    int          num_sub_;  // number of in-flight mapping subscriptions
    uint32_t     kidx_;     // schedule index
    int64_t      cts_;      // (re)connect timestamp
    int64_t      ctimeout_; // connection timeout
    uint64_t     slot_;     // current slot
    uint64_t     slot_cnt_; // slot count
    int64_t      curr_ts_;  // current time
    int64_t      pub_ts_;   // start publish time
    int64_t      pub_int_;  // publish interval
    kpx_vec_t    kvec_;     // symbol price scheduling
    bool         wait_conn_;// waiting on connection
    bool         do_cap_;   // do capture flag
    bool         do_tx_;    // do tx proxy connectivity
    bool         is_pub_;   // is publishing mode
    capture      cap_;      // aggregate price capture
    tx_parser    txp_;      // handle unexpected errors
    commitment   cmt_;      // account get/subscribe commitment

    // requests
    rpc::slot_subscribe        sreq_[1]; // slot subscription
    rpc::get_recent_block_hash breq_[1]; // block hash request
    rpc::program_subscribe     preq_[1]; // program account subscription
  };

  inline bool manager::get_is_tx_connect() const
  {
    return tconn_.get_is_connect();
  }

  inline void manager::write( pc_pub_key_t *key, pc_acc_t *ptr )
  {
    if ( do_cap_ ) {
      cap_.write( key, ptr );
    }
  }

}
