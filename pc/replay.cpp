#include "replay.hpp"

using namespace pc;

replay::replay()
: ts_( 0L ),
  up_( nullptr ),
  sz_( 0 )
{
}

void replay::set_file( const std::string& cap_file )
{
  mf_.set_file( cap_file );
}

std::string replay::get_file() const
{
  return mf_.get_file();
}

bool replay::init()
{
  if ( !mf_.init() ) {
    return set_err_msg( "failed to open/find file=" + mf_.get_file() );
  }
  sz_ = 0;
  return true;
}

struct pc_hdr
{
  int64_t  ts_;
  uint32_t magic_;
  uint32_t ver_;
  uint32_t size_;
  uint32_t ptype_;
};

bool replay::get_next()
{
  pc_hdr *hdr = (pc_hdr*)(mf_.data() + sz_);
  size_t left = mf_.size() - sz_;
  if ( left >= sizeof( pc_hdr ) && left >= hdr->size_ ) {
    ts_ = hdr->ts_;
    up_ = (pc_price_t*)&mf_.data()[sz_+sizeof(int64_t)];
    sz_ += hdr->size_;
    return true;
  } else {
    return false;
  }
}
