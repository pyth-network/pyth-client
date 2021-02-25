#include "net_socket.hpp"
#include "encode.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

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
: hd_( nullptr ),
  tl_( nullptr ),
  sz_( 0 )
{
}

net_wtr::~net_wtr()
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
  sz_ = 0;
  hd_ = tl_ = nullptr;
}

void net_wtr::alloc()
{
  net_buf *ptr = mem_.alloc();
  if ( tl_ ) {
    tl_->next_ = ptr;
    tl_ = ptr;
  } else {
    hd_ = tl_ = ptr;
  }
}

void net_wtr::add( const char *buf, size_t len )
{
  while( len>0 ) {
    if ( !tl_ || tl_->size_ == net_buf::len ) {
      alloc();
    }
    uint16_t left = net_buf::len - tl_->size_;
    size_t mlen = std::min( (size_t)left, len );
    __builtin_memcpy( &tl_->buf_[tl_->size_], buf, mlen );
    buf += mlen;
    len -= mlen;
    sz_ += mlen;
    tl_->size_ += mlen;
  }
}

void net_wtr::add( char val )
{
  if ( !tl_ || tl_->size_ == net_buf::len ) {
    alloc();
  }
  ++sz_;
  tl_->buf_[tl_->size_++] = val;
}

void net_wtr::add( const char *buf )
{
  add( buf, __builtin_strlen( buf ) );
}

void net_wtr::add( const std::string& buf )
{
  add( buf.c_str(), buf.length() );
}

void net_wtr::add( net_wtr& buf )
{
  net_buf *hd, *tl;
  buf.detach( hd, tl );
  if ( tl_ ) {
    tl_->next_ = hd;
  } else {
    hd_ = hd;
  }
  tl_ = tl;
}

///////////////////////////////////////////////////////////////////////////
// net_socket

net_socket::net_socket()
: fd_(-1),
  whd_( nullptr ),
  wtl_( nullptr ),
  rsz_( 0 ),
  wsz_( 0 )
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

void net_socket::close()
{
  if ( fd_ > 0 ) {
    ::close( fd_ );
    fd_ = -1;
  }
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

void net_socket::add_send( net_wtr& msg )
{
  net_buf *hd, *tl;
  msg.detach( hd, tl );
  if ( wtl_ ) {
    wtl_->next_ = hd;
  } else {
    whd_ = hd;
  }
  wtl_ = tl;
}

void net_socket::poll()
{
  if ( get_is_send() ) {
    poll_send();
  }
  poll_recv();
}

void net_socket::poll_send()
{
  if ( !whd_ || get_is_err() ) {
    return;
  }
  for(;;) {
    // write current buffer to ssl socket
    char *ptr = &whd_->buf_[wsz_];
    uint16_t len = whd_->size_ - wsz_;
    int rc = ::send( fd_, ptr, len, MSG_NOSIGNAL );
    if ( rc > 0 ) {
      wsz_ += rc;

      // advance to next buffer in list
      if ( wsz_ == whd_->size_ ) {
        net_buf *nxt = whd_->next_;
        whd_->dealloc();
        wsz_ = 0;
        if ( ! ( whd_ = nxt ) ) {
          wtl_ = nullptr;
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

void net_socket::poll_recv()
{
  while( !get_is_err() ) {
    // extend read buffer as required
    size_t len = rdr_.size() - rsz_;
    if ( len < buf_len ) {
      rdr_.resize( rdr_.size() + buf_len );
    }
    // read up to buf_len at a time
    ssize_t rc = ::recv( fd_, &rdr_[rsz_], buf_len, MSG_NOSIGNAL );
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
      if ( parse( &rdr_[idx], rsz_, rlen ) ) {
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

void net_socket::poll_error( bool is_read )
{
  std::string emsg = "fail to ";
  emsg += is_read?"read":"write";
  set_err_msg( emsg, errno );
}

///////////////////////////////////////////////////////////////////////////
// tcp_connect

tcp_connect::tcp_connect()
: port_(-1),
  conn_( nullptr )
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

void tcp_connect::set_net_socket( net_socket *conn )
{
  conn_ = conn;
}

net_socket *tcp_connect::get_net_socket() const
{
  return conn_;
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
  conn_->close();
  reset_err();
  int fd = ::socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  if ( fd < 0 ) {
    return set_err_msg( "failed to construct tcp socket", errno );
  }
  sockaddr saddr[1];
  if ( !get_hname_addr( host_, saddr ) ) {
    return set_err_msg( "failed to resolve host=" + host_ );
  }
  sockaddr_in *iaddr = (sockaddr_in*)saddr;
  iaddr->sin_port = htons( (uint16_t)port_ );
  if ( 0 != ::connect( fd, saddr, sizeof( sockaddr ) ) ) {
    return set_err_msg( "failed to connect to host=" + host_, errno );
  }
  conn_->reset_err();
  conn_->set_fd( fd );
  return true;
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
  add( txt, len );
  add( '\r' );
  add( '\n' );
}

void http_request::add_hdr( const char *hdr, const char *txt )
{
  add_hdr( hdr, txt, __builtin_strlen( txt ) );
}

void http_request::add_hdr( const char *hdr, uint64_t val )
{
  char buf[32];
  buf[31] = '\0';
  add_hdr( hdr, uint_to_str( val, &buf[31] ) );
}

void http_request::add_content( net_wtr& buf )
{
  add_hdr( "Content-Length", buf.size() );
  add( '\r' );
  add( '\n' );
  add( buf );
}

void http_request::add_content( const char *buf, size_t len )
{
  add_hdr( "Content-Length", len );
  add( '\r' );
  add( '\n' );
  add( buf, len );
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
      char *eptr[1] = { (char*)ptr };
      clen = strtol( val, eptr, 10 );
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

void http_client::parse_status( int status, const char *txt, size_t len )
{
  if ( status != 200 ) {
    set_err_msg( "http error: " + std::string( txt, len ) );
  }
}

void http_client::parse_header( const char *, size_t,
                                const char *, size_t )
{
}

void http_client::parse_content( const char *, size_t )
{
}
