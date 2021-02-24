#pragma once

#include <pc/error.hpp>
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
    size_t size() const;
    void detach( net_buf *&hd, net_buf *&tl );

  private:
    void alloc();
    net_buf *hd_;
    net_buf *tl_;
    size_t   sz_;
  };

  // socket-based network source
  class net_socket : public error
  {
  public:
    net_socket();

    // file descriptor access
    int get_fd() const;
    void set_fd( int );

    // close connection
    void close();

    // send/receive polling
    void poll_send();
    void poll_recv();
    virtual void poll();

    // add message to send queue
    void add_send( net_wtr& );

    // any messages in the send queue
    bool get_is_send() const;

    // set blocking on file descriptor
    bool set_block( bool );

    // parse inbound message
    virtual bool parse( char *buf, size_t sz, size_t& len ) = 0;

  private:

    typedef std::vector<char> buf_t;
    static const size_t buf_len = 2048;
    void poll_error( bool );

    int      fd_;    // socket
    bool     block_; // blocking flag
    buf_t    rdr_;   // inbound message read buffer
    net_buf *whd_;   // head of writer queue
    net_buf *wtl_;   // tail of writer queue
    size_t   rsz_;   // current read position
    uint16_t wsz_;   // current write position
  };

  // tcp connector base class
  class tcp_connect : public error
  {
  public:

    tcp_connect();

    // connection host
    void set_host( const std::string& );
    std::string get_host() const;

    // connection port
    void set_port( int port );
    int get_port() const;

    // associated connection
    void set_net_socket( net_socket * );
    net_socket *get_net_socket() const;

    // (re)connect to host
    bool init();

  private:
    int         port_; // connection port
    std::string host_; // connection host
    net_socket *conn_;
  };

  // http client connection
  class http_client : public net_socket,
                      public tcp_connect
  {
  };

  // websocket client connection
  class ws_socket : public net_socket
  {
  };

  class ws_client : public net_socket,
                    public tcp_connect
  {
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
