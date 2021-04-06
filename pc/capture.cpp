#include "capture.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace pc;

capture::capture()
: curr_( nullptr ),
  is_run_( true ),
  is_wtr_( false ),
  fd_(-1)
{
}

capture::~capture()
{
  flush();
  is_run_ = false;
  if ( thrd_.joinable() ) {
    thrd_.join();
  }
  for( cap_buf *ptr: reuse_ ) {
    free( (void*)ptr );
  }
  for( cap_buf *ptr: done_ ) {
    free( (void*)ptr );
  }
  reuse_.clear();
  done_.clear();
}

void capture::set_file( const std::string& file )
{
  file_ = file;
}

std::string capture::get_file() const
{
  return file_;
}

static void run_capture( capture *ptr )
{
  ptr->run();
}

bool capture::init()
{
  // check if file already exists
  struct stat fst[1];
  if ( 0 == ::stat( file_.c_str(), fst ) ) {
    return set_err_msg(
        "capture file already exists file=" + file_  );
  }
  fd_ = ::open( file_.c_str(), O_CREAT | O_WRONLY, 0644 );
  if ( fd_ < 0 ) {
    return set_err_msg(
        "failed to create capture file=" + file_, errno  );
  }
  thrd_ = std::thread( run_capture, this );
  return true;
}

capture::cap_buf *capture::alloc()
{
  if ( !reuse_.empty() ) {
    curr_ = reuse_.back();
    reuse_.pop_back();
  } else {
    curr_ = (cap_buf*)malloc( max_size );
  }
  curr_->size_ = 0;
  return curr_;
}

void capture::write( pc_price_t *ptr )
{
  if ( PC_UNLIKELY( !curr_ ) ) {
    curr_ = alloc();
  }
  uint64_t plen = sizeof( int64_t ) +
    ptr->size_ - sizeof( ptr->comp_ )  +
    ptr->num_ * sizeof( pc_price_comp_t );
  uint64_t left = max_size - curr_->size_ - sizeof( cap_buf );
  if ( PC_UNLIKELY( plen > left ) ) {
    flush();
    curr_ = alloc();
  }
  char *tgt = &curr_->buf_[curr_->size_];
  ((int64_t*)tgt)[0] = get_now();
  tgt += sizeof( int64_t );
  __builtin_memcpy( tgt, ptr, plen );
  ((pc_price_t*)tgt)->size_ = plen;
  curr_->size_ += plen;
}

void capture::flush()
{
  if ( !curr_ ) {
    return;
  }
  buf_vec_t done;
  mtx_.lock();
  pend_.push_back( curr_ );
  done_.swap( done );
  is_wtr_ = true;
  mtx_.unlock();
  curr_ = nullptr;
  std::copy( done.begin(), done.end(), std::back_inserter( reuse_ ) );
}

void capture::run()
{
  struct timespec ts[1];
  ts->tv_sec  = 0;
  ts->tv_nsec = 1000000;
  buf_vec_t pend;
  for(;;) {
    if ( is_wtr_ ) {
      // get buffers to write
      mtx_.lock();
      is_wtr_ = false;
      pend_.swap( pend );
      mtx_.unlock();

      // write to file
      for( cap_buf *ptr: pend ) {
        char *buf = ptr->buf_;
        size_t sz = ptr->size_;
        while( sz > 0 ) {
          int num = ::write( fd_, buf, sz );
          if ( num > 0 ) {
            buf += num;
            sz  -= num;
          } else {
            break;
          }
        }
      }

      // send buffers back to reuse
      mtx_.lock();
      std::copy( pend.begin(), pend.end(), std::back_inserter( done_ ) );
      mtx_.unlock();
      pend.clear();

    } else if ( !is_run_ ) {
      break;
    } else {
      // sleep a bit
      clock_nanosleep( CLOCK_REALTIME, 0, ts, NULL );
    }
  }
}
