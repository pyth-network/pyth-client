#pragma once

#include <pc/error.hpp>
#include <pc/jtree.hpp>
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
    void add( const char *buf, size_t len );
    void add( const char *buf );
    void add( const std::string& buf );
    void add( net_wtr& );
    size_t size() const;
    void detach( net_buf *&hd, net_buf *&tl );

  protected:
    void alloc();
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

  // socket-based network source
  class net_socket : public error
  {
  public:
    net_socket();

    // file descriptor access
    int get_fd() const;
    void set_fd( int );

    // set socket blocking flag
    bool set_block( bool block );

    // associated parser
    void set_net_parser( net_parser * );
    net_parser *get_net_parser() const;

    // close connection
    void close();

    // initialize
    virtual bool init();

    // send/receive polling
    void poll_send();
    void poll_recv();
    virtual void poll();

    // add message to send queue
    void add_send( net_wtr& );

    // any messages in the send queue
    bool get_is_send() const;

  protected:

    typedef std::vector<char> buf_t;
    static const size_t buf_len = 2048;
    void poll_error( bool );

    int         fd_;  // socket
    buf_t       rdr_; // inbound message read buffer
    net_buf    *whd_; // head of writer queue
    net_buf    *wtl_; // tail of writer queue
    size_t      rsz_; // current read position
    uint16_t    wsz_; // current write position
    net_parser *np_;  // message parser
  };

  // tcp connector or client
  class tcp_connect : public net_socket
  {
  public:

    tcp_connect();

    // connection host
    void set_host( const std::string& );
    std::string get_host() const;

    // connection port
    void set_port( int port );
    int get_port() const;

    // (re)connect to host
    bool init() override;

  private:
    int         port_; // connection port
    std::string host_; // connection host
  };

  // http request message
  class http_request : public net_wtr
  {
  public:
    void init( const char *method="POST", const char *endpoint="/" );
    void add_hdr( const char *hdr, const char *txt, size_t txt_len );
    void add_hdr( const char *hdr, const char *txt  );
    void add_hdr( const char *hdr, uint64_t val );
    void add_content( const char *, size_t );
    void add_content( net_wtr& );
    void add_content();
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

    void commit( uint8_t opcode, const char *, size_t len, bool mask );
  };

  // websocket
  class ws_connect : public tcp_connect
  {
  public:
    bool init() override;

  private:
    struct ws_connect_init : public http_client {
      void parse_status( int, const char *, size_t) override;
      ws_connect *cp_;
      net_parser *tp_;
    };
    ws_connect_init init_;
  };

  // websocket protocol impl
  class ws_parser : public net_parser
  {
  public:
    ws_parser();

    // connection to send control message replies
    void set_net_socket( net_socket * );
    net_socket *get_net_socket() const;

    // parse websocket protocol
    bool parse( const char *buf, size_t sz, size_t& len ) override;

    // callback on websocket message
    virtual void parse_msg( const char *buf, size_t sz );

  protected:
    typedef std::vector<char> buf_t;
    buf_t       msg_;
    net_socket *wptr_;
  };

  inline bool net_socket::get_is_send() const
  {
    return whd_ != nullptr;
  }

  inline size_t net_wtr::size() const
  {
    return sz_;
  }

}
