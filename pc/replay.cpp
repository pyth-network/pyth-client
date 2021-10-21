#include "replay.hpp"

using namespace pc;

static const size_t buf_sz = 1024*1024;

replay::replay()
: up_( nullptr ),
  buf_( nullptr ),
  pos_( 0 ),
  len_( 0 ),
  zfd_( nullptr )
{
  buf_ = new char[buf_sz];
}

replay::~replay()
{
  if ( zfd_ ) {
    ::gzclose( zfd_ );
    zfd_ = nullptr;
  }
  if ( buf_ ) {
    delete [] buf_;
    buf_ = nullptr;
  }
}

void replay::set_file( const std::string& cap_file )
{
  file_ = cap_file;
}

std::string replay::get_file() const
{
  return file_;
}

bool replay::init()
{
  std::string file = file_;
  size_t flen = file.length();
  if ( flen >=3 && file.substr(flen-3) != ".gz" ) {
    file += ".gz";
  }
  zfd_ = ::gzopen( file.c_str(), "r" );
  if ( !zfd_ ) {
    return set_err_msg( "failed to open file=" + file );
  }
  static const size_t gzb_sz = 128*1024;
  if ( 0 != gzbuffer( zfd_, gzb_sz ) ) {
    return set_err_msg(
        "failed to set compression buffer file=" + file );
  }
  pos_ = len_ = 0;
  return true;
}

bool replay::get_next()
{
  for(;;) {
    size_t left = len_ - pos_;
    up_ = (hdr*)&buf_[pos_];
    size_t upsz = sizeof( hdr );
    if ( left >= upsz ) {
      upsz = sizeof( int64_t ) + sizeof( pc_pub_key_t ) + up_->acc_.size_;
    }
    if ( left >= upsz ) {
      pos_ += upsz;
      return true;
    } else {
      if ( pos_ ) {
        __builtin_memmove( &buf_[0], &buf_[pos_], left );
      }
      pos_ = 0;
      len_ = left;
      int numread = ::gzread( zfd_, &buf_[len_], buf_sz - len_ );
      if ( numread > 0 ) {
        len_ += static_cast< size_t >( numread );
      } else {
        return false;
      }
    }
  }
}
