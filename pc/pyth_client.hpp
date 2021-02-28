#pragma once

#include <pc/net_socket.hpp>
#include <pc/rpc_client.hpp>
#include <pc/prev_next.hpp>
#include <pc/dbl_list.hpp>

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

  // pyth-client connection management
  class pyth_server : public error,
                      public net_accept
  {
  public:

    pyth_server();

    // solana rpc http connection
    void set_rpc_host( const std::string& );
    std::string get_rpc_host() const;

    // server listening port
    void set_listen_port( int port );
    int get_listen_port() const;

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

  private:

    typedef dbl_list<pyth_client> clnt_list_t;
    void teardown_clients();

    net_loop    nl_;    // epoll loop
    tcp_connect hconn_; // rpc http connection
    tcp_connect wconn_; // rpc websocket sonnection
    tcp_listen  lsvr_;  // listening socket
    rpc_client  clnt_;  // rpc api
    clnt_list_t olist_; // open clients list
    clnt_list_t dlist_; // to-be-deleted client list
    std::string rhost_; // rpc host
  };

}
