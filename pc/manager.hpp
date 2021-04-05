#pragma once

#include <pc/net_socket.hpp>
#include <pc/rpc_client.hpp>
#include <pc/request.hpp>
#include <pc/user.hpp>
#include <pc/key_store.hpp>
#include <pc/dbl_list.hpp>
#include <pc/hash_map.hpp>

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

    // on completion of (re)bootstrap of accounts following (re)connect
    virtual void on_init( manager * );

    // on addition of new symbols
    virtual void on_add_symbol( manager *, price * );
  };

  // pyth-client connection management and event loop
  class manager : public key_store,
                  public net_accept,
                  public rpc_sub,
                  public rpc_sub_i<rpc::slot_subscribe>,
                  public rpc_sub_i<rpc::get_recent_block_hash>
  {
  public:

    manager();
    virtual ~manager();

    // solana rpc http connection
    void set_rpc_host( const std::string& );
    std::string get_rpc_host() const;

    // server listening port
    void set_listen_port( int port );
    int get_listen_port() const;

    // server subscription version
    void set_version( uint32_t );
    uint32_t get_version() const;

    // content directory (for http content requests if running as server)
    void set_content_dir( const std::string& );
    std::string get_content_dir() const;

    // event subscription callback
    void set_manager_sub( manager_sub * );
    manager_sub *get_manager_sub() const;

    // rpc client interface
    rpc_client *get_rpc_client();

    // recent block hash stuff
    hash *get_recent_block_hash();

    // get most recently processed slot
    uint64_t get_slot() const;

    // slot start time esitimate
    int64_t get_slot_time() const;

    // slot interval time estimate
    int64_t get_slot_interval() const;

    // add and subscribe to new mapping account
    void add_mapping( const pub_key& );

    // add new symbol from mapping account
    void add_symbol( const pub_key& );
    void add_symbol( const symbol&, price_type, price * );
    price *get_symbol( const symbol&, price_type );

    // iterate through symbols
    unsigned get_num_symbol() const;
    price *get_symbol( unsigned i ) const;

    // submit pyth client api request
    void submit( request * );

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

    // rpc callbacks
    void on_response( rpc::slot_subscribe * );
    void on_response( rpc::get_recent_block_hash * );
    void set_status( int );

  private:

    struct symbol_key {
      symbol     sym_;
      price_type ptype_;
      bool operator==( const symbol_key& obj ) const {
        return sym_ == obj.sym_ && ptype_ == obj.ptype_;
      }
    };

    struct trait_symbol {
      static const size_t hsize_ = 8363UL;
      typedef uint32_t           idx_t;
      typedef symbol_key         key_t;
      typedef const symbol_key&  keyref_t;
      typedef price        *val_t;
      struct hash_t {
        idx_t operator() ( keyref_t s ) {
          uint64_t *i = (uint64_t*)s.sym_.data();
          return ( (i[0]^i[1]) << 3 ) | (uint64_t)s.ptype_;
        }
      };
    };

    struct trait_account {
      static const size_t hsize_ = 8363UL;
      typedef uint32_t        idx_t;
      typedef pub_key         key_t;
      typedef const pub_key&  keyref_t;
      typedef price     *val_t;
      struct hash_t {
        idx_t operator() ( keyref_t a ) {
          uint64_t *i = (uint64_t*)a.data();
          return *i;
        }
      };
    };

    typedef dbl_list<user>            user_list_t;
    typedef dbl_list<request>         req_list_t;
    typedef std::vector<get_mapping*> map_vec_t;
    typedef std::vector<price*>       spx_vec_t;
    typedef std::vector<price_sched*> kpx_vec_t;
    typedef hash_map<trait_symbol>    sym_map_t;
    typedef hash_map<trait_account>   acc_map_t;

    void reconnect_rpc();
    void log_disconnect();
    void teardown_users();
    void reset_status( int );

    net_loop     nl_;       // epoll loop
    tcp_connect  hconn_;    // rpc http connection
    ws_connect   wconn_;    // rpc websocket sonnection
    tcp_listen   lsvr_;     // listening socket
    rpc_client   clnt_;     // rpc api
    user_list_t  olist_;    // open users list
    user_list_t  dlist_;    // to-be-deleted users list
    req_list_t   plist_;    // pending requests
    map_vec_t    mvec_;     // mapping account updates
    sym_map_t    smap_;     // symbol+ptype to symbol pricing info
    acc_map_t    amap_;     // account to symbol pricing info
    spx_vec_t    svec_;     // symbol price subscriber/publishers
    std::string  rhost_;    // rpc host
    std::string  cdir_;     // content directory
    manager_sub *sub_;      // subscription callback
    int          status_;   // status bitmap
    int          num_sub_;  // number of in-flight mapping subscriptions
    uint32_t     version_;  // account version subscription
    uint32_t     kidx_;     // schedule index
    int64_t      cts_;      // (re)connect timestamp
    int64_t      ctimeout_; // connection timeout
    int64_t      slot_ts_;  // slot start time estimate
    int64_t      slot_int_; // slot interval
    int64_t      slot_min_; // slot minimum interval
    uint64_t     slot_;     // current slot
    uint64_t     slot_cnt_; // slot count
    int64_t      cum_ack_;  // cumulative block hash ack times
    int64_t      num_ack_;  // number of block hash acks
    kpx_vec_t    kvec_;     // symbol price scheduling
    bool         wait_conn_;// waiting on connection

    // requests
    rpc::slot_subscribe        sreq_[1]; // slot subscription
    rpc::get_recent_block_hash breq_[1]; // block hash request
  };

}
