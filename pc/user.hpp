#pragma once

#include <pc/net_socket.hpp>
#include <pc/rpc_client.hpp>
#include <pc/request.hpp>
#include <pc/key_store.hpp>
#include <pc/dbl_list.hpp>

namespace pc
{

  class manager;

  // pyth daemon web-socket user connection
  class user : public prev_next<user>,
               public net_connect,
               public ws_parser,
               public request_sub,
               public request_sub_i<price>,
               public request_sub_i<price_sched>
  {
  public:
    user();

    // associated rpc connection
    void set_rpc_client( rpc_client * );

    // associated pyth server
    void set_manager( manager * );

    // http request message parsing
    void parse_content( const char *, size_t );

    // websocket message parsing
    void parse_msg( const char *buf, size_t sz ) override;

    // manager disconnected
    void teardown() override;

    // symbol update callback
    void on_response( price *, uint64_t ) override;

    // symbol price schedule callback
    void on_response( price_sched *, uint64_t ) override;

  private:

    // http-only request parsing
    struct user_http : public http_server {
      void parse_content( const char *, size_t ) override;
      user *ptr_;
    };

    struct deferred_sub {
      price   *sptr_;
      uint64_t sid_;
    };

    typedef std::vector<deferred_sub> def_vec_t;

    void parse_request( uint32_t );
    void parse_get_product_list( uint32_t );
    void parse_get_product( uint32_t, uint32_t );
    void parse_get_all_products( uint32_t );
    void parse_upd_price( uint32_t,  uint32_t );
    void parse_sub_price( uint32_t,  uint32_t );
    void parse_sub_price_sched( uint32_t,  uint32_t );
    void add_header();
    void add_tail( uint32_t id );
    void add_parse_error();
    void add_invalid_request( uint32_t id = 0 );
    void add_invalid_params( uint32_t id );
    void add_unknown_symbol( uint32_t id );
    void add_error( uint32_t id, int err, str );

    rpc_client     *rptr_;        // rpc manager api
    manager        *sptr_;        // manager collection
    user_http       hsvr_;        // http parser
    jtree           jp_;          // json parser
    json_wtr        jw_;          // json writer
    def_vec_t       dvec_;        // deferred subscriptions
    request_sub_set psub_;        // price subscriptions
  };

}
