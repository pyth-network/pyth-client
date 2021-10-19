#include "mem_map.hpp"

#include <assert.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

using namespace pc;

mem_map::mem_map()
: fd_(-1),
  len_( 0 ),
  buf_( nullptr )
{
}

mem_map::~mem_map()
{
  close();
}

void mem_map::close()
{
  if ( buf_ ) {
    ::munmap( (void*)buf_, len_ );
    buf_ = nullptr;
  }
  if ( fd_ > 0 ) {
    ::close( fd_ );
    fd_ = -1;
  }
}

void mem_map::set_file( const std::string& filen )
{
  file_ = filen;
}

std::string mem_map::get_file() const
{
  return file_;
}

bool mem_map::remap()
{
  struct stat fst[1];
  if ( 0 != fstat( fd_, fst ) || fst->st_size == 0 ) {
    return false;
  }
  assert( fst->st_size >= 0 );
  size_t nlen = static_cast< size_t >( fst->st_size );
  if ( nlen == len_ ) {
    return false;
  }
  void *buf = ::mremap( (void*)buf_, len_, nlen, MREMAP_MAYMOVE );
  if ( buf == MAP_FAILED ) {
    return false;
  }
  buf_ = (const char*)buf;
  len_ = nlen;
  return true;
}

bool mem_map::init()
{
  close();
  int fd = ::open( file_.c_str(), O_RDONLY );
  if ( fd <= 0 ) {
    return false;
  }
  struct stat fst[1];
  if ( 0 != fstat( fd, fst ) || fst->st_size == 0 ) {
    ::close( fd );
    return false;
  }
  assert( fst->st_size >= 0 );
  len_ = static_cast< size_t >( fst->st_size );
  void *buf = mmap( NULL, len_, PROT_READ, MAP_SHARED, fd, 0 );
  if ( buf == MAP_FAILED ) {
    return false;
  }
  buf_ = (const char*)buf;
  fd_ = fd;
  return true;
}
