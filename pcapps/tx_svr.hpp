#pragma once

#include "tx_rpc_client.hpp"
#include <pc/net_socket.hpp>
#include <pc/rpc_client.hpp>
#include <pc/dbl_list.hpp>

namespace pc
{

  class tx_svr;

  class tx_user : public prev_next<tx_user>,
                  public net_connect,
                  public net_parser
  {
  public:
    tx_user();
    void set_tx_svr( tx_svr * );
    // tx update
    bool parse( const char *buf, size_t sz, size_t& len ) override;
    // tx_svr disconnected
    void teardown() override;
  private:
    tx_svr *mgr_;
  };

  // tx_svr server run as a busy loop
  class tx_svr : public error,
                 public net_accept,
                 public rpc_sub,
                 public rpc_sub_i<rpc::get_health>,
                 public rpc_sub_i<rpc::slot_subscribe>,
                 public rpc_sub_i<rpc::get_cluster_nodes>,
                 public rpc_sub_i<rpc::get_slot_leaders>
  {
  public:

    tx_svr();
    virtual ~tx_svr();

    // solana rpc http connection
    void set_rpc_host( const std::string& );
    std::string get_rpc_host() const;

    // server listening port
    void set_listen_port( int port );
    int get_listen_port() const;

    // initialize
    bool init();

    // poll for tx_svr updates
    void poll(bool do_wait = true);

    // teardown all connections
    void teardown();

    // net_accept callback
    void accept( int ) override;

    // move user to teardown list
    void del_user( tx_user *usr );

    // send tpu request
    void submit( const char *buf, size_t len );

    // rpc calbacks
    void on_response( rpc::slot_subscribe * ) override;
    void on_response( rpc::get_cluster_nodes * ) override;
    void on_response( rpc::get_slot_leaders * ) override;
    void on_response( rpc::get_health * ) override;

  private:

    typedef dbl_list<tx_user>    user_list_t;
    typedef std::vector<ip_addr> addr_vec_t;

    void reconnect_rpc();
    void log_disconnect();
    void teardown_users();
    void add_addr( const ip_addr& );

    static const size_t buf_len = 2048;

    bool         has_conn_;    // rpc connected flag
    bool         wait_conn_;   // wait for rpc connect flag
    net_loop     nl_;          // epoll loop
    rpc_client   clnt_;        // rpc API
    char        *msg_;         // message buffer
    ip_addr      src_[1];      // src ip address
    uint64_t     slot_;        // current slot
    uint64_t     slot_cnt_;    // number of slots received
    addr_vec_t   avec_;        // address vector of leaders
    tcp_connect  hconn_;       // rpc http connection
    ws_connect   wconn_;       // rpc websocket sonnection
    udp_socket   tconn_;       // udp sending socket
    tcp_listen   tsvr_;        // tpu listening socket
    user_list_t  olist_;       // open users list
    user_list_t  dlist_;       // to-be-deleted users list
    int64_t      cts_;         // (re)connect timestamp
    int64_t      ctimeout_;    // connection timeout
    std::string  rhost_;       // rpc host

    // rpc subscription info
    rpc::slot_subscribe    sreq_[1];
    rpc::get_cluster_nodes creq_[1];
    rpc::get_slot_leaders  lreq_[1];
    rpc::get_health        hreq_[1];
  };


}
