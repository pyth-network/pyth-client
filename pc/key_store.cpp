#include "key_store.hpp"
#include "net_socket.hpp"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace pc;

static bool write_key_file(
    const std::string& key_file, const key_pair& kp )
{
  json_wtr wtr;
  wtr.add_val( kp );
  net_buf *hd, *tl;
  wtr.detach( hd, tl );
  int fd = ::open( key_file.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0600 );
  if ( fd<0 ) {
    return false;
  }
  char *buf = hd->buf_;
  size_t len = hd->size_;
  do {
    int num = ::write( fd, buf, len );
    if ( num < 0 ) {
      ::close( fd );
      hd->dealloc();
      return false;
    }
    len -= num;
    buf += num;
  } while( len );
  fchmod( fd, 0400 );
  ::close( fd );
  hd->dealloc();
  return true;
}

key_store::key_store()
: has_pkey_( false ),
  has_mkey_( false ),
  has_mpub_( false ),
  has_gkey_( false ),
  has_gpub_( false )
{
}

void key_store::set_dir( const std::string& dirn )
{
  dir_ = dirn;
}

std::string key_store::get_dir() const
{
  return dir_;
}

bool key_store::create()
{
  struct stat fst[1];
  if ( 0 == ::stat( dir_.c_str(), fst ) ) {
    if ( 0 != chmod( dir_.c_str(), 0700 ) ) {
      return set_err_msg( "failed to chmod key_store directory", errno );
    }
  } else {
    if ( 0 != mkdir( dir_.c_str(), 0700 ) ) {
      return set_err_msg( "failed to create key_store directory", errno );
    }
  }
  return true;
}

bool key_store::init()
{
  has_pkey_ = has_mkey_ = has_mpub_ = has_gkey_ = has_gpub_ = false;
  struct stat fst[1];
  if ( 0 != ::stat( dir_.c_str(), fst ) ) {
    return set_err_msg( "cant find key_store directory", errno );
  }
  if ( !S_ISDIR(fst->st_mode) ) {
    return set_err_msg( "key_store path not a directory" );
  }
  if ( fst->st_uid != getuid() || fst->st_uid != geteuid() ) {
    return set_err_msg( "user must own the key_store directory" );
  }
  if ( ! (fst->st_mode & S_IRUSR ) ) {
    return set_err_msg(
        "user must have read access to key_store directory");
  }
  if ( ! (fst->st_mode & S_IWUSR ) ) {
    return set_err_msg(
        "user must have write access to key_store directory");
  }
  if ( fst->st_mode & ( S_IRWXG | S_IRWXO ) ) {
    return set_err_msg( "key store directory must be restricted to user "
       "read/write permissions" );
  }
  if ( dir_.back() != '/' ) {
    dir_ += '/';
  }
  return true;
}

std::string key_store::get_publish_key_pair_file() const
{
  return dir_ + "publish_key_pair.json";
}

std::string key_store::get_mapping_key_pair_file() const
{
  return dir_ + "mapping_key_pair.json";
}

std::string key_store::get_mapping_pub_key_file() const
{
  return dir_ + "mapping_key.json";
}

std::string key_store::get_program_key_pair_file() const
{
  return dir_ + "program_key_pair.json";
}

std::string key_store::get_program_pub_key_file() const
{
  return dir_ + "program_key.json";
}

key_pair *key_store::create_publish_key_pair()
{
  pkey_.gen();
  if ( !write_key_file( get_publish_key_pair_file(), pkey_ ) ) {
    return nullptr;
  }
  pkey_.get_pub_key( ppub_ );
  has_pkey_ = true;
  return &pkey_;
}

key_pair *key_store::get_publish_key_pair()
{
  if ( has_pkey_ ) {
    return &pkey_;
  }
  if ( !pkey_.init_from_file( get_publish_key_pair_file() ) ) {
    return nullptr;
  }
  has_pkey_ = true;
  ckey_.set( pkey_ );
  pkey_.get_pub_key( ppub_ );
  return &pkey_;
}

key_cache *key_store::get_publish_key_cache()
{
  if ( get_publish_key_pair() ) {
    return &ckey_;
  } else {
    return nullptr;
  }
}

pub_key *key_store::get_publish_pub_key()
{
  if ( has_pkey_ ) {
    return &ppub_;
  }
  if ( get_publish_key_pair() ) {
    return &ppub_;
  }
  return nullptr;
}

key_pair *key_store::create_mapping_key_pair()
{
  mkey_.gen();
  if ( !write_key_file( get_mapping_key_pair_file(), mkey_ ) ) {
    return nullptr;
  }
  mkey_.get_pub_key( mpub_ );
  has_mkey_ = true;
  has_mpub_ = true;
  return &mkey_;
}

key_pair *key_store::get_mapping_key_pair()
{
  if ( has_mkey_ ) {
    return &mkey_;
  }
  if ( !mkey_.init_from_file( get_mapping_key_pair_file() ) ) {
    return nullptr;
  }
  mkey_.get_pub_key( mpub_ );
  has_mkey_ = true;
  has_mpub_ = true;
  return &mkey_;
}

pub_key *key_store::get_mapping_pub_key()
{
  if ( has_mpub_ ) {
    return &mpub_;
  }
  if ( get_mapping_key_pair() ) {
    return &mpub_;
  }
  if ( !mpub_.init_from_file( get_mapping_pub_key_file() ) ) {
    return nullptr;
  }
  has_mpub_ = true;
  return &mpub_;
}

key_pair *key_store::create_program_key_pair()
{
  gkey_.gen();
  if ( !write_key_file( get_program_key_pair_file(), gkey_ ) ) {
    return nullptr;
  }
  gkey_.get_pub_key( gpub_ );
  has_gkey_ = true;
  has_gpub_ = true;
  return &gkey_;
}

key_pair *key_store::get_program_key_pair()
{
  if ( has_gkey_ ) {
    return &gkey_;
  }
  if ( !gkey_.init_from_file( get_program_key_pair_file() ) ) {
    return nullptr;
  }
  gkey_.get_pub_key( gpub_ );
  has_gkey_ = true;
  has_gpub_ = true;
  return &gkey_;
}

pub_key *key_store::get_program_pub_key()
{
  if ( has_gpub_ ) {
    return &gpub_;
  }
  if ( get_program_key_pair() ) {
    return &gpub_;
  }
  if ( !gpub_.init_from_file( get_program_pub_key_file() ) ) {
    return nullptr;
  }
  has_gpub_ = true;
  return &gpub_;
}

bool key_store::create_account_key_pair( key_pair& res )
{
  res.gen();
  pub_key pk( res );
  std::string filen;
  pk.enc_base58( filen );
  filen = dir_ + "account_" + filen + ".json";
  if ( !write_key_file( filen, res ) ) {
    return false;
  }
  return true;
}

bool key_store::get_account_key_pair( const pub_key& pk, key_pair& res )
{
  std::string filen;
  pk.enc_base58( filen );
  filen = dir_ + "account_" + filen + ".json";
  if ( res.init_from_file( filen ) ) {
    pub_key chk( res );
    return chk == pk;
  } else {
    return false;
  }
}


