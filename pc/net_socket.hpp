#pragma once

#include <pc/error.hpp>
#include <pc/key_pair.hpp>
#include <pc/misc.hpp>
#include <sys/epoll.h>
#include <vector>

namespace pc
{

  // network message buffer
  struct net_buf
  {
    static const uint16_t len = 1270;
    net_buf *next_;
    uint16_t size_;
    char     buf_[len];
    void dealloc();
    static net_buf *alloc();
  };

  // network message writer
  class net_wtr
  {
  public:
    net_wtr();
    ~net_wtr();
    void add( char );
    void add( str );
    void add( net_wtr& );
    void detach( net_buf *&hd, net_buf *&tl );
    size_t size() const;
    void print() const;
    void reset();

  protected:
    void add_alloc( str );
    void alloc();
    void dealloc();
    void advance( size_t len );
    char *reserve( size_t len );
    net_buf *hd_;
    net_buf *tl_;
    size_t   sz_;
  };

  // parse inbound fragmented messages from streaming protocols
  class net_parser : public error
  {
  public:
    virtual ~net_parser();

    // parse inbound message
    virtual bool parse( const char *buf, size_t sz, size_t& len ) = 0;
  };

  class net_socket;

  // epoll-based loop
  class net_loop : public error
  {
  public:
    net_loop();
    ~net_loop();

    // initialize
    bool init();

    // add/delete sockets to epoll loop
    void add( net_socket *, int events );
    void del( net_socket * );

    // poll all connected sockets
    bool poll( int timeout );

  private:

    static const int max_events_ = 128;

    int         fd_;                 // epoll file descriptor
    epoll_event ev_[1];              // event used in epoll_ctl
    epoll_event evarr_[max_events_]; // receive events
  };

  // socket-based network source
  class net_socket : public error
  {
  public:

    net_socket();
    virtual ~net_socket();

    // file descriptor access
    int get_fd() const;
    void set_fd( int );

    // associated epoll loop
    void set_net_loop( net_loop * );
    net_loop *get_net_loop() const;

    // close connection
    void close();

    // set socket blocking flag
    bool set_block( bool block );

    // flag indicating if part of net_loop
    void set_in_loop( bool );
    bool get_in_loop() const;

    // initialize
    virtual bool init();

    // poll socket
    virtual void poll();

    // teardown connection and clean-up
    virtual void teardown();

  private:
    int        fd_;  // socket
    bool       inl_; // in-loop flag
    net_loop  *lp_;  // optional event_loop
  };

  // read/write client connection
  class net_connect : public net_socket
  {
  public:
    net_connect();

    // associated parser
    void set_net_parser( net_parser * );
    net_parser *get_net_parser() const;

    // send/receive polling
    void poll() override;
    void poll_send();
    void poll_recv();

    // add message to send queue
    void add_send( net_wtr& );

    // any messages in the send queue
    bool get_is_send() const;

    // drop all outbound messages
    void teardown() override;

  protected:

    typedef std::vector<char> buf_t;
    static const size_t buf_len = 2048;
    void poll_error( bool );

    buf_t       rdr_; // inbound message read buffer
    net_buf    *whd_; // head of writer queue
    net_buf    *wtl_; // tail of writer queue
    size_t      rsz_; // current read position
    uint16_t    wsz_; // current write position
    net_parser *np_;  // message parser
  };

  // new client acceptor for net_listen
  class net_accept
  {
  public:
    virtual ~net_accept();
    virtual void accept( int fd ) = 0;
  };

  // listening server
  class net_listen : public net_socket
  {
  public:
    net_listen();

    // new client acceptor
    void set_net_accept( net_accept * );
    net_accept *get_net_accept() const;

    // connection backlog
    void set_backlog( int );
    int get_backlog() const;

    // start listening
    bool init() override;

    // accept new clients
    void poll() override;

  private:
    static const int default_backlog = 8;

    int         backlog_;
    net_accept *ap_;
  };

  // tcp connector or client
  class tcp_connect : public net_connect
  {
  public:

    tcp_connect();

    // connection host
    void set_host( const std::string& );
    std::string get_host() const;

    // connection port
    void set_port( int port );
    int get_port() const;

    // timeout trying to connect
    void set_timeout( int64_t );
    int64_t get_timeout() const;

    // (re)connect to host
    bool init() override;

    // teardown connection
    void teardown() override;

    // check connection status
    virtual void check();

    // are we waiting to connect
    virtual bool get_is_wait();

  private:
    int         port_; // connection port
    bool        wait_; // is waiting to connect
    std::string host_; // connection host
    int64_t     sts_;  // start connect time
    int64_t     timeout_;
  };

  // listening tcp server
  class tcp_listen : public net_listen
  {
  public:
    tcp_listen();

    // listening port
    void set_port( int port );
    int get_port() const;

    bool init() override;

  private:
    int port_; // listening port
  };

  // http request message
  class http_request : public net_wtr
  {
  public:
    void init( const char *method="POST", const char *endpoint="/" );
    void add_hdr( const char *hdr, str );
    void add_hdr( const char *hdr, uint64_t val );
    void commit( net_wtr& );
    void commit();
  };

  // http client parser
  class http_client : public net_parser
  {
  public:
    bool parse( const char *buf, size_t sz, size_t& len ) override;
    virtual void parse_status( int, const char *, size_t);
    virtual void parse_header( const char *hdr, size_t hdr_len,
                               const char *val, size_t val_len);
    virtual void parse_content( const char *content, size_t content_len );
  };

  class http_response : public http_request
  {
  public:
    void init( const char *code, const char *msg );
  };

  class ws_parser;

  // http server parser
  class http_server : public net_parser
  {
  public:
    http_server();

    // associated net_connect for submitting upgrade message
    void set_net_connect( net_connect * );
    net_connect *get_net_connect() const;

    // websocket parser on connection upgrade
    void set_ws_parser( ws_parser * );
    ws_parser *get_ws_parser() const;

    // parse http request
    bool parse( const char *buf, size_t sz, size_t& len ) override;

    // called on new request message
    virtual void parse_content( const char *, size_t );

    // access to http request components
    unsigned get_num_header() const;
    void get_header_key( unsigned i, str& ) const;
    void get_header_val( unsigned i, str&) const;
    bool get_header_val( const str&key, str&) const;
    void get_path( str& ) const;

  private:

    typedef std::vector<str> str_vec_t;

    void upgrade_ws();

    str          path_;
    str_vec_t    hnms_;
    str_vec_t    hval_;
    ws_parser   *wp_;
    net_connect *np_;
  };

  // websocket message builder
  class ws_wtr : public net_wtr
  {
  public:
    // websocket op-codes
    static const uint8_t cont_id   = 0x0;
    static const uint8_t text_id   = 0x1;
    static const uint8_t binary_id = 0x2;
    static const uint8_t close_id  = 0x8;
    static const uint8_t ping_id   = 0x9;
    static const uint8_t pong_id   = 0xa;

    void commit( uint8_t opcode, net_wtr&, bool mask );
  };

  // websocket client connection
  class ws_connect : public tcp_connect
  {
  public:
    ws_connect();
    bool init() override;
    void check() override;
    bool get_is_wait() override;

  private:
    struct ws_connect_init : public http_client {
      void parse_status( int, const char *, size_t) override;
      ws_connect *cp_;
      net_parser *np_;
      bool        hs_;
    };
    ws_connect_init init_;
  };

  // websocket client protocol impl
  class ws_parser : public net_parser
  {
  public:
    ws_parser();

    // connection to send control message replies
    void set_net_connect( net_connect * );
    net_connect *get_net_connect() const;

    // parse websocket protocol
    bool parse( const char *buf, size_t sz, size_t& len ) override;

    // callback on websocket message
    virtual void parse_msg( const char *buf, size_t sz );

  protected:
    typedef std::vector<char> buf_t;
    buf_t        msg_;
    net_connect *wptr_;
  };

  class json_wtr : public net_wtr
  {
  public:

    typedef enum { e_obj = 0, e_arr } type_t;

    struct null {};
    struct jtrue {};
    struct jfalse {};

    json_wtr();
    void reset();

    // pop latest object/array
    void pop();

    // add key/value pair in object
    void add_key( str key, str val );
    void add_key( str key, int64_t val );
    void add_key( str key, uint64_t val );
    void add_key( str key, type_t );
    void add_key( str key, const hash& );
    void add_key( str key, null );
    void add_key( str key, jtrue );
    void add_key( str key, jfalse );
    void add_key_verbatim( str key, str );

    // add array value
    void add_val( str val );
    void add_val( uint64_t );
    void add_val( int64_t );
    void add_val( type_t );
    void add_val( const key_pair& );
    void add_val( const hash& );
    void add_val( const signature& );
    void add_val_enc_base58( str val );
    void add_val_enc_base64( str val );

  private:

    typedef std::vector<type_t> type_vec_t;

    void add_key_only( str key );
    void add_obj();
    void add_arr();
    void add_first();
    void add_int( int64_t ival );
    void add_uint( uint64_t ival );
    void add_enc_base58( str );
    void add_enc_base64( str );
    void add_text( str );

    bool       first_;
    type_vec_t st_;
  };

  /////////////////////////////////////////////////////////////////////////
  // inline impl.

  inline unsigned http_server::get_num_header() const
  {
    return hnms_.size();
  }

  inline void http_server::get_header_key( unsigned i, str& val ) const
  {
    val = hnms_[i];
  }

  inline void http_server::get_header_val( unsigned i, str& val ) const
  {
    val = hval_[i];
  }

  inline void http_server::get_path( str& val ) const
  {
    val = path_;
  }

}
