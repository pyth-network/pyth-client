#include "net_socket.hpp"
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
