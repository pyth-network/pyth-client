#include "net_socket.hpp"
#include <openssl/sha.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <iostream>

#define PC_EPOLL_FLAGS (EPOLLIN|EPOLLET|EPOLLRDHUP|EPOLLHUP|EPOLLERR)

namespace pc
{
  // net_buf allocation and caching scheme
  struct net_buf_alloc
  {
  public:
    net_buf_alloc();
    ~net_buf_alloc();
    net_buf *alloc();
    void dealloc( net_buf * );
  private:
    net_buf *ptr_;
  };

}

using namespace pc;

///////////////////////////////////////////////////////////////////////////
// net_buf_alloc

net_buf_alloc::net_buf_alloc()
: ptr_( nullptr )
{
  static_assert( sizeof( net_buf ) == 1280);
}

net_buf_alloc::~net_buf_alloc()
{
  while( ptr_ ) {
    net_buf *nxt = ptr_->next_;
    delete ptr_;
    ptr_ = nxt;
  }
}

net_buf *net_buf_alloc::alloc()
{
  net_buf *res;
  if ( ptr_ ) {
    res  = ptr_;
    ptr_ = res->next_;
  } else {
    res = new net_buf;
  }
  res->next_ = nullptr;
  res->size_ = 0;
  return res;
}

void net_buf_alloc::dealloc( net_buf *ptr )
{
  ptr->next_ = ptr_;
  ptr_ = ptr;
}

static net_buf_alloc mem_;

net_buf *net_buf::alloc()
{
  return mem_.alloc();
}

void net_buf::dealloc()
{
  mem_.dealloc( this );
}

///////////////////////////////////////////////////////////////////////////
// net_wtr


net_wtr::net_wtr()
: hd_( mem_.alloc() ),
  tl_( hd_ ),
  sz_( 0 )
{
}

net_wtr::~net_wtr()
{
  dealloc();
}

void net_wtr::reset()
{
  dealloc();
  hd_ = tl_ = mem_.alloc();
  sz_ = 0;
}

void net_wtr::dealloc()
{
  for( net_buf *ptr = hd_; ptr; ) {
    net_buf *nxt = ptr->next_;
    ptr->dealloc();
    ptr = nxt;
  }

}

void net_wtr::detach( net_buf *&hd, net_buf *&tl )
{
  hd  = hd_;
  tl  = tl_;
  hd_ = tl_ = nullptr;
  sz_ = 0UL;
}

void net_wtr::alloc()
{
  net_buf *ptr = mem_.alloc();
  tl_->next_ = ptr;
  sz_ += tl_->size_;
  tl_ = ptr;
}

void net_wtr::add( str str )
{
  size_t nlen = tl_->size_ + str.len_;
  if ( nlen <= net_buf::len ) {
    __builtin_memcpy( &tl_->buf_[tl_->size_], str.str_, str.len_ );
    tl_->size_ = nlen;
  } else {
    add_alloc( str );
  }
}

void net_wtr::add( char val )
{
  if ( PC_UNLIKELY( tl_->size_ == net_buf::len ) ) {
    alloc();
  }
  tl_->buf_[tl_->size_++] = val;
}

void net_wtr::add( net_wtr& buf )
{
  net_buf *hd, *tl;
  sz_ += tl_->size_ + buf.size();
  buf.detach( hd, tl );
  if ( tl_->size_ + hd->size_ <= net_buf::len ) {
    __builtin_memcpy( &tl_->buf_[tl_->size_], hd->buf_, hd->size_ );
    tl_->next_ = hd->next_;
    tl_->size_ += hd->size_;
    if ( hd != tl ) {
      tl_ = tl;
    }
    hd->dealloc();
  } else {
    tl_->next_ = hd;
    tl_ = tl;
  }
  sz_ -= tl->size_;
}

void net_wtr::add_alloc( str str )
{
  while( str.len_ >0 ) {
    if ( tl_->size_ == net_buf::len ) {
      alloc();
    }
    size_t left = net_buf::len - tl_->size_;
    size_t mlen = std::min( left, str.len_ );
    __builtin_memcpy( &tl_->buf_[tl_->size_], str.str_, mlen );
    tl_->size_ += mlen;
    str.str_ += mlen;
    str.len_ -= mlen;
  }
}

char *net_wtr::reserve( size_t len )
{
  size_t nlen = tl_->size_ + len;
  if ( nlen > net_buf::len ) {
    alloc();
  }
  return &tl_->buf_[tl_->size_];
}

void net_wtr::advance( size_t len )
{
  tl_->size_ += len;
}

size_t net_wtr::size() const
{
  return sz_ + tl_->size_;
}

void net_wtr::print() const
{
  for( net_buf *ptr=hd_; ptr; ptr = ptr->next_ ) {
    std::cout.write( ptr->buf_, ptr->size_ );
  }
  std::cout << std::endl;
}

///////////////////////////////////////////////////////////////////////////
// net_loop

net_loop::net_loop()
: fd_(-1)
{
  __builtin_memset( ev_, 0, sizeof( ev_ ) );
  __builtin_memset( evarr_, 0, sizeof( evarr_ ) );
}

net_loop::~net_loop()
{
  if ( fd_ > 0 ) {
    ::close( fd_ );
    fd_ = -1;
  }
}

bool net_loop::init()
{
  fd_ = ::epoll_create( 1 );
  if ( fd_ < 0 ) {
    return set_err_msg( "failed to create epoll", errno );
  }
  return true;
}

void net_loop::add( net_socket *eptr, int events )
{
  ev_->events   = events;
  ev_->data.ptr = eptr;
  int evop = EPOLL_CTL_ADD;
  if ( eptr->get_in_loop() ) {
    evop = EPOLL_CTL_MOD;
  } else {
    eptr->set_in_loop( true );
  }
  epoll_ctl( fd_, evop, eptr->get_fd(), ev_ );
}

void net_loop::del( net_socket *eptr )
{
  if ( eptr->get_in_loop() ) {
    ev_->events   = 0;
    ev_->data.ptr = eptr;
    epoll_ctl( fd_, EPOLL_CTL_DEL, eptr->get_fd(), ev_ );
    eptr->set_in_loop( false );
  }
}

bool net_loop::poll( int timeout )
{
  int nfds = epoll_wait( fd_, evarr_, max_events_, timeout );
  if ( nfds > 0 ) {
    for(int i=0; i != nfds; ++i ) {
      epoll_event& ev = evarr_[i];
      net_socket *sp = static_cast<net_socket*>( ev.data.ptr );
      sp->poll();
      if ( sp->get_is_err() ) {
        del( sp );
        sp->teardown();
      }
    }
    return true;
  } else {
    return false;
  }
}

///////////////////////////////////////////////////////////////////////////
// net_socket

net_parser::~net_parser()
{
}

net_socket::~net_socket()
{
}

net_socket::net_socket()
: fd_(-1),
  inl_( false ),
  lp_( nullptr )
{
}

int net_socket::get_fd() const
{
  return fd_;
}

void net_socket::set_fd( int fd )
{
  fd_ = fd;
}

void net_socket::set_net_loop( net_loop *lp )
{
  lp_ = lp;
}

net_loop *net_socket::get_net_loop() const
{
  return lp_;
}

void net_socket::set_in_loop( bool inl )
{
  inl_ = inl;
}

bool net_socket::get_in_loop() const
{
  return inl_;
}

void net_socket::close()
{
  if ( fd_ > 0 ) {
    if ( lp_ ) {
      lp_->del( this );
    }
    ::close( fd_ );
    fd_ = -1;
  }
}

void net_socket::poll()
{
}

void net_socket::teardown()
{
  close();
}

bool net_socket::set_block( bool block )
{
  int flags = ::fcntl( fd_, F_GETFL, 0 );
  if ( flags < 0 ){
    return set_err_msg( "fcntl() failed", errno );
  }
  if ( block ) {
    flags &= ~O_NONBLOCK;
  } else {
    flags |= O_NONBLOCK;
  }
  if ( 0 > ::fcntl( fd_, F_SETFL, flags ) ) {
    return set_err_msg( "fcntl() failed", errno );
  }
  return true;
}

bool net_socket::init()
{
  if ( lp_ ) {
    lp_->add( this, PC_EPOLL_FLAGS );
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////
// net_connect

net_connect::net_connect()
: whd_( nullptr ),
  wtl_( nullptr ),
  rsz_( 0 ),
  wsz_( 0 ),
  np_( nullptr )
{
}

void net_connect::set_net_parser( net_parser *np )
{
  np_ = np;
}

net_parser *net_connect::get_net_parser() const
{
  return np_;
}

bool net_connect::get_is_send() const
{
  return whd_ != nullptr;
}

void net_connect::add_send( net_wtr& msg )
{
  net_buf *hd, *tl;
  msg.detach( hd, tl );
  if ( wtl_ ) {
    wtl_->next_ = hd;
  } else {
    whd_ = hd;
    if ( get_net_loop() ) {
      get_net_loop()->add( this, PC_EPOLL_FLAGS | EPOLLOUT );
    }
  }
  wtl_ = tl;
}

void net_connect::poll()
{
  if ( get_is_send() ) {
    poll_send();
  }
  poll_recv();
}

void net_connect::poll_send()
{
  if ( !whd_ || get_is_err() ) {
    return;
  }
  for(;;) {
    // write current buffer to ssl socket
    char *ptr = &whd_->buf_[wsz_];
    uint16_t len = whd_->size_ - wsz_;
    int rc = ::send( get_fd(), ptr, len, MSG_NOSIGNAL );
    if ( rc > 0 ) {
      wsz_ += rc;

      // advance to next buffer in list
      if ( wsz_ == whd_->size_ ) {
        net_buf *nxt = whd_->next_;
        whd_->dealloc();
        wsz_ = 0;
        if ( ! ( whd_ = nxt ) ) {
          wtl_ = nullptr;
          if ( get_net_loop() ) {
            get_net_loop()->add( this, PC_EPOLL_FLAGS );
          }
          break;
        }
      }
    } else {
      // check if this is not a try again sort of error
      if ( rc == 0 || errno != EAGAIN ) {
        poll_error( true );
      }
      break;
    }
  }
}

void net_connect::poll_recv()
{
  while( !get_is_err() ) {
    // extend read buffer as required
    size_t len = rdr_.size() - rsz_;
    if ( len < buf_len ) {
      rdr_.resize( rdr_.size() + buf_len );
    }
    // read up to buf_len at a time
    ssize_t rc = ::recv( get_fd(), &rdr_[rsz_], buf_len, MSG_NOSIGNAL );
    if ( rc > 0 ) {
      rsz_ += rc;
    } else {
      if ( rc == 0 || errno != EAGAIN ) {
        poll_error( true );
      }
      break;
    }
    // parse content
    for( size_t idx=0; !get_is_err() && rsz_; ) {
      size_t rlen = 0;
      if ( np_->parse( &rdr_[idx], rsz_, rlen ) ) {
        idx  += rlen;
        rsz_ -= rlen;
      } else {
        // shuffle remaining bytes to beginning of buffer
        if ( idx ) {
          __builtin_memmove( &rdr_[0], &rdr_[idx], rsz_ );
        }
        break;
      }
    }
  }
}

void net_connect::poll_error( bool is_read )
{
  std::string emsg = "fail to ";
  emsg += is_read?"read":"write";
  set_err_msg( emsg, errno );
}

///////////////////////////////////////////////////////////////////////////
// net_listen

net_accept::~net_accept()
{
}

net_listen::net_listen()
: backlog_( default_backlog ),
  ap_( nullptr )
{
}

void net_listen::set_net_accept( net_accept *ap )
{
  ap_ = ap;
}

net_accept *net_listen::get_net_accept() const
{
  return ap_;
}

void net_listen::set_backlog( int backlog )
{
  backlog_ = backlog;
}

int net_listen::get_backlog() const
{
  return backlog_;
}

bool net_listen::init()
{
  if ( 0 > ::listen( get_fd(), backlog_ ) ) {
    return set_err_msg( "failed to listen", errno );
  }
  return set_block( false ) && net_socket::init();
}

void net_listen::poll()
{
  struct sockaddr addr[1];
  socklen_t addrlen[1] = { 0 };
  for(;;) {
    int nfd = ::accept( get_fd(), addr, addrlen );
    if ( nfd>0 ) {
      ap_->accept( nfd );
    } else {
      if ( errno != EAGAIN ) {
        set_err_msg( "failed to accept new client", errno );
      }
      break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////
// tcp_connect

tcp_connect::tcp_connect()
: port_(-1)
{
}

void tcp_connect::set_host( const std::string& hostn )
{
  host_ = hostn;
}

std::string tcp_connect::get_host() const
{
  return host_;
}

void tcp_connect::set_port( int port )
{
  port_ = port;
}

int tcp_connect::get_port() const
{
  return port_;
}

static bool get_hname_addr( const std::string& name, sockaddr *saddr )
{
  memset( saddr, 0, sizeof( sockaddr ) );
  bool has_addr = false;
  addrinfo hints[1];
  memset( hints, 0, sizeof( addrinfo ) );
  hints->ai_family   = AF_INET;
  hints->ai_socktype = SOCK_STREAM;
  hints->ai_protocol = IPPROTO_TCP;
  addrinfo *ainfo[1] = { nullptr };
  if ( 0 > ::getaddrinfo( name.c_str(), nullptr, nullptr, ainfo ) ) {
    return false;
  }
  for( addrinfo *aptr = ainfo[0]; aptr; aptr = aptr->ai_next ) {
    if ( aptr->ai_family == hints->ai_family &&
         aptr->ai_socktype == hints->ai_socktype &&
         aptr->ai_protocol == hints->ai_protocol ) {
      __builtin_memcpy( saddr, aptr->ai_addr, sizeof( sockaddr ) );
      has_addr = true;
      break;
    }
  }
  if ( ainfo[0] ) {
    ::freeaddrinfo( ainfo[0] );
  }
  return has_addr;
}

bool tcp_connect::init()
{
  close();
  reset_err();
  int fd = ::socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  if ( fd < 0 ) {
    return set_err_msg( "failed to construct tcp socket", errno );
  }
  sockaddr saddr[1];
  if ( !get_hname_addr( host_, saddr ) ) {
    return set_err_msg( "failed to resolve host" );
  }
  sockaddr_in *iaddr = (sockaddr_in*)saddr;
  iaddr->sin_port = htons( (uint16_t)port_ );
  if ( 0 != ::connect( fd, saddr, sizeof( sockaddr ) ) ) {
    return set_err_msg( "failed to connect", errno );
  }
  set_fd( fd );
  set_block( false );
  return net_socket::init();
}

///////////////////////////////////////////////////////////////////////////
// tcp_listen

tcp_listen::tcp_listen()
: port_( -1 )
{
}

void tcp_listen::set_port( int port )
{
  port_ = port;
}

int tcp_listen::get_port() const
{
  return port_;
}

bool tcp_listen::init()
{
  close();
  reset_err();
  int fd = ::socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  if ( fd < 0 ) {
    return set_err_msg( "failed to construct tcp socket", errno );
  }
  int reuse[1] = { 1 };
  if ( 0 > ::setsockopt(
        fd, SOL_SOCKET, SO_REUSEADDR, reuse, sizeof( reuse ) ) ) {
    ::close( fd );
    return set_err_msg( "failed to set socket reuse address", errno );
  }
  sockaddr saddr[1];
  memset( saddr, 0, sizeof( sockaddr ) );
  sockaddr_in *iaddr = (sockaddr_in*)saddr;
  iaddr->sin_port = htons( (uint16_t)port_ );
  if ( 0> ::bind( fd, saddr, sizeof( sockaddr ) ) ) {
    ::close( fd );
    return set_err_msg(
        "failed to bind to port=" + std::to_string(port_), errno );
  }
  if ( port_ == 0 ) {
    socklen_t slen[1] = { sizeof( saddr ) };
    getsockname( fd, saddr, slen );
    port_ = htons( iaddr->sin_port );
  }
  set_fd( fd );
  return net_listen::init();
}

///////////////////////////////////////////////////////////////////////////
// http_request

void http_request::init( const char *method, const char *endpoint )
{
  add( method );
  add( ' ' );
  add( endpoint );
  add( " HTTP/1.1\r\n" );
}

void http_request::add_hdr( const char *hdr, const char *txt, size_t len )
{
  add( hdr );
  add( ':' );
  add( ' ' );
  add( str( txt, len ) );
  add( '\r' );
  add( '\n' );
}

void http_request::add_hdr( const char *hdr, const char *txt )
{
  add_hdr( hdr, txt, __builtin_strlen( txt ) );
}

void http_request::add_hdr( const char *hdr, uint64_t ival )
{
  add( hdr );
  add( ':' );
  add( ' ' );
  char *buf = reserve( 32 ), *end = &buf[31];
  char *val = uint_to_str( ival, end );
  size_t val_len = end - val;
  __builtin_memmove( buf, val, val_len );
  advance( val_len );
  add( '\r' );
  add( '\n' );
}

void http_request::commit( net_wtr& buf )
{
  add_hdr( "Content-Length", buf.size() );
  add( '\r' );
  add( '\n' );
  add( buf );
}

void http_request::commit()
{
  add_hdr( "Content-Length", "0" );
  add( '\r' );
  add( '\n' );
}

///////////////////////////////////////////////////////////////////////////
// http_client

static inline bool find( const char ch, const char *&ptr, const char *end )
{
  for(;ptr!=end;++ptr) {
    if ( *ptr == ch ) return true;
  }
  return false;
}

static inline bool next(char ch,  const char *ptr, const char *end )
{
  return ptr != end && *ptr == ch;
}

bool http_client::parse( const char *ptr, size_t len, size_t& res )
{
  const char CR = (char)13;
  const char LF = (char)10;

  // read status line in response
  const char *beg = ptr;
  const char *end = &ptr[len];
  if ( !find( ' ', ptr, end ) ) return false;
  const char *stp = ++ptr;
  if ( !find( ' ', ptr, end ) ) return false;
  char *eptr[1] = { (char*)ptr };
  int status = strtol( stp, eptr, 10 );
  stp = ++ptr;
  if ( !find( CR, ptr, end ) )  return false;
  parse_status( status, stp, ptr - stp );
  if ( !next( LF, ++ptr, end ) )  return false;

  // parse other header lines
  bool has_len = false;
  size_t clen = 0;
  for(++ptr;;++ptr) {
    if ( ptr <= &end[-2] && ptr[0] == CR && ptr[1] == LF ) {
      break;
    }
    const char *hdr = ptr;
    if ( !find( ':', ptr, end ) ) return false;
    const char *hdr_end = ptr;
    for( ++ptr; ptr != end && isspace(*ptr); ++ptr );
    const char *val = ptr;
    if ( !find( CR, ptr, end ) )  return false;
    if ( has_len || (
          0 != __builtin_strncmp( "Content-Length", hdr, hdr_end-hdr ) &&
          0 != __builtin_strncmp( "content-length", hdr, hdr_end-hdr ) ) ) {
      parse_header( hdr, hdr_end-hdr, val, ptr-val );
    } else {
      has_len = true;
      clen = str_to_uint( val, ptr - val );
    }
    if ( !next( LF, ++ptr, end ) )  return false;
  }
  // parse body
  ptr += 2;
  const char *cnt = &ptr[clen];
  if ( cnt > end ) return false;

  parse_content( ptr, clen );
  // assign total message size
  res = cnt - beg;
  return true;
}

void http_client::parse_status( int, const char *, size_t )
{
}

void http_client::parse_header( const char *, size_t,
                                const char *, size_t )
{
}

void http_client::parse_content( const char *, size_t )
{
}

///////////////////////////////////////////////////////////////////////////
// http_server

void http_response::init( const char *code, const char *msg )
{
  add( "HTTP/1.1 " );
  add( code );
  add( ' ' );
  add( msg );
  add( '\r' );
  add( '\n' );
}

http_server::http_server()
: wp_( nullptr ),
  np_( nullptr )
{
}

void http_server::set_net_connect( net_connect *np )
{
  np_ = np;
}

net_connect *http_server::get_net_connect() const
{
  return np_;
}

void http_server::set_ws_parser( ws_parser *wp )
{
  wp_ = wp;
}

ws_parser *http_server::get_ws_parser() const
{
  return wp_;
}

inline http_server::str::str()
: txt_( nullptr ), len_( 0 )
{
}

inline http_server::str::str( const char *txt, size_t len )
: txt_( txt ), len_( len ) {
}

inline void http_server::str::set( const char *txt, size_t len )
{
  txt_ = txt;
  len_ = len;
}

inline void http_server::str::get( const char *&txt, size_t& len ) const
{
  txt = txt_;
  len = len_;
}

bool http_server::get_header_val(
    const char *key, const char *&txt, size_t&len ) const
{
  for(unsigned i=0; i != hnms_.size(); ++i ) {
    const str& h = hnms_[i];
    if ( 0 == __builtin_strncmp( key, h.txt_, h.len_ ) ) {
      hval_[i].get( txt, len );
      return true;
    }
  }
  return false;
}

bool http_server::parse( const char *ptr, size_t len, size_t& res )
{
  const char CR = (char)13;
  const char LF = (char)10;

  // read request line
  const char *beg = ptr;
  const char *end = &ptr[len];
  if ( !find( ' ', ptr, end ) ) return false;
  const char *path = ++ptr;
  if ( !find( ' ', ptr, end ) ) return false;
  size_t path_len = ptr - path;
  if ( !find( CR, ptr, end ) )  return false;
  path_.set( path, path_len );
  if ( !next( LF, ++ptr, end ) )  return false;

  // parse other header lines
  bool has_len = false, has_upgrade = false;;
  size_t clen = 0;
  for(++ptr;;++ptr) {
    if ( ptr <= &end[-2] && ptr[0] == CR && ptr[1] == LF ) {
      break;
    }
    const char *hdr = ptr;
    if ( !find( ':', ptr, end ) ) return false;
    const char *hdr_end = ptr;
    for( ++ptr; ptr != end && isspace(*ptr); ++ptr );
    const char *val = ptr;
    if ( !find( CR, ptr, end ) )  return false;
    size_t hlen = hdr_end-hdr;
    if ( has_len || (
        0 != __builtin_strncmp( "Content-Length", hdr, hlen ) &&
        0 != __builtin_strncmp( "content-length", hdr, hlen ) ) ) {
      hnms_.push_back( str( hdr, hlen ) );
      hval_.push_back( str( val, ptr-val ) );
      if ( wp_ && !has_upgrade ) {
        has_upgrade =
          0 == __builtin_strncmp( "Upgrade", hdr, hlen ) &&
          0 == __builtin_strncmp( "websocket", val, ptr-val );
      }
    } else {
      has_len = true;
      clen = str_to_uint( val, ptr - val );
    }
    if ( !next( LF, ++ptr, end ) )  return false;
  }
  // parse body
  ptr += 2;
  const char *cnt = &ptr[clen];
  if ( cnt > end ) return false;

  // assign total message size
  res = cnt - beg;

  // upgrade or parse request
  if ( !has_upgrade ) {
    parse_content( ptr, clen );
  } else {
    upgrade_ws();
  }

  // downstream parsing
  return true;
}

void http_server::upgrade_ws()
{
  size_t key_len;
  const char *key;
  if ( !get_header_val( "Sec-WebSocket-Key", key, key_len ) ) {
    return;
  }
  const char *magic_id = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  char skey[SHA_DIGEST_LENGTH];
  __builtin_memset( skey, 0, SHA_DIGEST_LENGTH );
  SHA_CTX cx[1];
  SHA1_Init( cx );
  SHA1_Update( cx, key, key_len );
  SHA1_Update( cx, magic_id, __builtin_strlen( magic_id ) );
  SHA1_Final( (uint8_t*)skey, cx );
  char bkey[SHA_DIGEST_LENGTH+SHA_DIGEST_LENGTH];
  size_t blen = enc_base64(
      (const uint8_t*)skey, SHA_DIGEST_LENGTH, (uint8_t*)bkey );

  // submit switch protocols message
  http_response msg;
  msg.init( "101", "Switching Protocols" );
  msg.add_hdr( "Connection", "Upgrade" );
  msg.add_hdr( "Upgrade", "websocket" );
  msg.add_hdr( "Sec-WebSocket-Accept", bkey, blen );
  msg.commit();
  np_->add_send( msg );

  // switch parser
  np_->set_net_parser( wp_ );
  wp_->set_net_connect( np_ );
}

void http_server::parse_content( const char *, size_t )
{
}

///////////////////////////////////////////////////////////////////////////
// ws_connect

bool ws_connect::init()
{
  if ( !tcp_connect::init() ) {
    return false;
  }
  init_.tp_ = get_net_parser();
  init_.cp_ = this;
  set_net_parser( &init_ );

  // request upgrade to web-socket
  http_request msg;
  msg.init( "GET", "/" );
  msg.add_hdr( "Connection", "Upgrade" );
  msg.add_hdr( "Sec-WebSocket-Key", "dGhlIHNhbXBsZSBub25jZQ==" );
  msg.add_hdr( "Sec-WebSocket-Version", "13" );
  msg.commit();
  add_send( msg );
  return true;
}

void ws_connect::ws_connect_init::parse_status(
    int status, const char *txt, size_t len)
{
  if ( status == 101 ) {
    cp_->set_net_parser( tp_ );
  } else {
    std::string err = "failed to handshake websocket: ";
    err.append( txt, len );
    cp_->set_err_msg( err );
  }
}

///////////////////////////////////////////////////////////////////////////
// ws_wtr

struct PC_PACKED ws_hdr1
{
  uint8_t op_code_:4;
  uint8_t rsv3_:1;
  uint8_t rsv2_:1;
  uint8_t rsv1_:1;
  uint8_t fin_:1;
  uint8_t pay_len1_:7;
  uint8_t mask_:1;
};

struct PC_PACKED ws_hdr2 : public ws_hdr1
{
  uint16_t pay_len2_;
};

struct PC_PACKED ws_hdr3 : public ws_hdr1
{
  uint64_t pay_len3_;
};

void ws_wtr::commit( uint8_t op_code, net_wtr& buf, bool mask )
{
  size_t pay_len = buf.size();
  char *hdr = reserve( sizeof( ws_hdr3 ) + sizeof( uint32_t ) );
  size_t hdsz = 0;
  ws_hdr1 *hptr1 = (ws_hdr1*)hdr;
  hptr1->fin_  = 1;
  hptr1->rsv1_ = 0;
  hptr1->rsv2_ = 0;
  hptr1->rsv3_ = 0;
  hptr1->mask_ = mask;
  hptr1->op_code_ = op_code;
  if ( pay_len < 126 ) {
    hptr1->pay_len1_ = pay_len;
    hdsz = sizeof( ws_hdr1 );
  } else if ( pay_len <= 0xffff ) {
    hptr1->pay_len1_ = 126;
    ws_hdr2 *hptr2 = (ws_hdr2*)hdr;
    hptr2->pay_len2_ = __builtin_bswap16( (uint16_t)pay_len );
    hdsz = sizeof( ws_hdr2 );
  } else {
    hptr1->pay_len1_ = 127;
    ws_hdr3 *hptr3 = (ws_hdr3*)hdr;
    hptr3->pay_len3_ = __builtin_bswap64( (uint64_t)pay_len );
    hdsz = sizeof( ws_hdr3 );
  }
  // generate mask
  if ( mask ) {
    uint32_t mask_key = random();
    const char *mptr = &hdr[hdsz];
    ((uint32_t*)mptr)[0] = mask_key;
    hdsz += sizeof( uint32_t );
    advance( hdsz );
    add( buf );
    unsigned i=0;
    for( net_buf *ptr = hd_; ptr; ptr = ptr->next_ ) {
      for( char *ibuf = &ptr->buf_[hdsz], *ebuf = &ptr->buf_[ptr->size_];
           ibuf != ebuf; ++ibuf, ++i ) {
        *ibuf ^= mptr[i%4];
      }
      hdsz = 0;
    }
  } else {
    advance( hdsz );
    add( buf );
  }
}

///////////////////////////////////////////////////////////////////////////
// ws_parser

ws_parser::ws_parser()
: wptr_( nullptr )
{
}

void ws_parser::set_net_connect( net_connect *wptr )
{
  wptr_ = wptr;
}

net_connect *ws_parser::get_net_connect() const
{
  return wptr_;
}

bool ws_parser::parse( const char *ptr, size_t len, size_t& res )
{
  if ( len < sizeof( ws_hdr1 ) ) return false;
  char *payload = (char*)ptr;
  ws_hdr1 *hptr1 = (ws_hdr1*)ptr;
  uint64_t pay_len = 0, msk_len = hptr1->mask_ ? 4 : 0;
  if ( hptr1->pay_len1_ < 126 ) {
    pay_len = hptr1->pay_len1_;
    payload += sizeof( ws_hdr1 );
    if ( len < sizeof( ws_hdr1 ) + pay_len + msk_len ) return false;
  } else if ( hptr1->pay_len1_ == 126 ) {
    if ( len < sizeof( ws_hdr2) ) return false;
    ws_hdr2 *hptr2 = (ws_hdr2*)ptr;
    payload += sizeof( ws_hdr2 );
    pay_len = __builtin_bswap16( hptr2->pay_len2_);
    if ( len < sizeof( ws_hdr2 ) + pay_len + msk_len ) return false;
  } else {
    if ( len < sizeof( ws_hdr3) ) return false;
    ws_hdr3 *hptr3 = (ws_hdr3*)ptr;
    payload += sizeof( ws_hdr3 );
    pay_len = __builtin_bswap64( hptr3->pay_len3_ );
    size_t tot_sz = sizeof( ws_hdr3 ) + pay_len + msk_len;
    if ( len < tot_sz ) return false;
  }
  if ( msk_len ) {
    const char *mask = payload;
    payload += msk_len;
    for(unsigned i=0; i != pay_len;++i ) {
      payload[i] = payload[i] ^ mask[i%4];
    }
  }
  res = pay_len + ( payload - ptr );
  switch( hptr1->op_code_ ) {
    case ws_wtr::text_id:
    case ws_wtr::binary_id:{
      if ( hptr1->fin_ ) {
        parse_msg( payload, pay_len );
      } else {
        msg_.insert( msg_.end(), payload, &payload[pay_len] );
      }
      break;
    }
    case ws_wtr::cont_id:{
      msg_.insert( msg_.end(), payload, &payload[pay_len] );
      if ( hptr1->fin_ ) {
        parse_msg( msg_.data(), msg_.size() );
        msg_.clear();
      }
      break;
    }
    case ws_wtr::ping_id:{
      net_wtr ping;
      ping.add( str( payload, pay_len ) );
      ws_wtr msg;
      msg.commit( ws_wtr::pong_id, ping, msk_len==0 );
      wptr_->add_send( msg );
      break;
    }
    case ws_wtr::pong_id:{
      break;
    }
    case ws_wtr::close_id:{
      net_wtr cmsg;
      ws_wtr msg;
      msg.commit( ws_wtr::close_id, cmsg, msk_len==0 );
      wptr_->add_send( msg );
      break;
    }
    default:{
      set_err_msg( "unknown op_code=" +
          std::to_string((unsigned)hptr1->op_code_ ) );
      break;
    }
  }
  return true;
}

void ws_parser::parse_msg( const char *, size_t )
{
}

///////////////////////////////////////////////////////////////////////////
// json_wtr

json_wtr::json_wtr()
: first_( true )
{
}

void json_wtr::reset()
{
  net_wtr::reset();
  first_ = true;
  st_.clear();
}

void json_wtr::add_obj()
{
  add( '{' );
  first_ = true;
  st_.push_back( e_obj );
}

void json_wtr::add_arr()
{
  add( '[' );
  first_ = true;
  st_.push_back( e_arr );
}

void json_wtr::pop()
{
  add( st_.back() == e_obj ? '}' : ']' );
  st_.pop_back();
  first_ = false;
}

void json_wtr::add_first()
{
  if ( !first_ ) add( ',' );
  first_ = false;
}

void json_wtr::add_key_only( str key )
{
  add_first();
  add_text( key );
  add( ':' );
}

void json_wtr::add_key( str key, str val )
{
  add_key_only( key );
  add_text( val );
}

void json_wtr::add_key( str key, int64_t ival )
{
  add_key_only( key );
  add_int( ival );
}

void json_wtr::add_key( str key, uint64_t ival )
{
  add_key_only( key );
  add_uint( ival );
}

void json_wtr::add_key( str key, null )
{
  add_key_only( key );
  add( "null" );
}

void json_wtr::add_key_verbatim( str key, str val )
{
  add_key_only( key );
  add( val );
}

void json_wtr::add_key( str key, type_t t )
{
  add_key_only( key );
  if ( t == e_obj ) {
    add_obj();
  } else {
    add_arr();
  }
}

void json_wtr::add_val( const key_pair& kp )
{
  add_val( e_arr );
  for( unsigned i=0; i!=key_pair::len; ++i ) {
    add_val( (uint64_t)kp.data()[i] );
  }
  pop();
}

void json_wtr::add_key( str key, const hash& pk )
{
  add_key_only( key );
  add_enc_base58( str( pk.data(), hash::len ) );
}

void json_wtr::add_val( str val )
{
  add_first();
  add_text( val );
}

void json_wtr::add_val( const hash& pk )
{
  add_val_enc_base58( str( pk.data(), hash::len ) );
}

void json_wtr::add_val( const signature& sig )
{
  add_val_enc_base58( str( sig.data(), signature::len ) );
}

void json_wtr::add_val( uint64_t ival )
{
  add_first();
  add_uint( ival );
}

void json_wtr::add_val( type_t t )
{
  add_first();
  if ( t == e_obj ) {
    add_obj();
  } else {
    add_arr();
  }
}

void json_wtr::add_val_enc_base58( str val )
{
  add_first();
  add_enc_base58( val );
}

void json_wtr::add_val_enc_base64( str val )
{
  add_first();
  add_enc_base64( val );
}

void json_wtr::add_uint( uint64_t ival )
{
  char *buf = reserve( 32 ), *end = &buf[31];
  char *val = uint_to_str( ival, end );
  size_t val_len = end - val;
  __builtin_memmove( buf, val, val_len );
  advance( val_len );
}

void json_wtr::add_int( int64_t ival )
{
  char *buf = reserve( 32 ), *end = &buf[31];
  char *val = int_to_str( ival, end );
  size_t val_len = end - val;
  __builtin_memmove( buf, val, val_len );
  advance( val_len );
}

void json_wtr::add_enc_base58( str val )
{
  add( '"' );
  size_t rsv_len = val.len_ + val.len_;
  char *tgt = reserve( rsv_len );
  advance( enc_base58( (const uint8_t*)val.str_,
        val.len_, (uint8_t*)tgt, rsv_len ) );
  add( '"' );
}

void json_wtr::add_enc_base64( str val )
{
  add( '"' );
  size_t rsv_len = enc_base64_len( val.len_ );
  char *tgt = reserve( rsv_len );
  advance( enc_base64( (const uint8_t*)val.str_,
        val.len_, (uint8_t*)tgt ) );
  add( '"' );
}

void json_wtr::add_text( str val )
{
  add( '"' );
  add( val );
  add( '"' );
}
