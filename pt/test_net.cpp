#include "test_error.hpp"
#include <pc/net_socket.hpp>

using namespace pc;

void test_net_buf()
{
  {
    net_wtr msg;
    msg.add( "hello" );
    PC_TEST_CHECK( msg.size() == 5 );
  }
  {
    net_wtr msg;
    msg.add( "hello" );
    net_buf *hd, *tl;
    msg.detach( hd, tl );
    PC_TEST_CHECK( hd==tl );
    PC_TEST_CHECK( hd->next_ == nullptr );
    PC_TEST_CHECK( tl->size_ == 5 );
    hd->dealloc();
  }
  {
    net_wtr msg;
    char buf[1460];
    __builtin_memset( buf, 1, 1460 );
    msg.add( buf, 1460 );
    PC_TEST_CHECK( msg.size() == 1460 );
    net_buf *hd, *tl;
    msg.detach( hd, tl );
    PC_TEST_CHECK( hd->next_==tl );
    PC_TEST_CHECK( tl->next_==nullptr );
    PC_TEST_CHECK( hd->size_ == net_buf::len );
    hd->dealloc();
    tl->dealloc();
  }
  {
    net_wtr msg;
    msg.add( 'H' );
    msg.add( 'e' );
    msg.add( "llo" );
    PC_TEST_CHECK( msg.size() == 5 );
    net_buf *hd, *tl;
    msg.detach( hd, tl );
    PC_TEST_CHECK( hd==tl );
    PC_TEST_CHECK( hd->next_ == nullptr );
    PC_TEST_CHECK( tl->size_ == 5 );
    PC_TEST_CHECK( 0 == __builtin_strncmp( "Hello", tl->buf_, 5 ) );
    hd->dealloc();

    msg.add( "one" );
    msg.add( "two" );
    msg.add( "three" );
    msg.add( '\0' );
    msg.detach( hd, tl );
    PC_TEST_CHECK( 0 == __builtin_strcmp( "onetwothree", tl->buf_ ) );
    hd->dealloc();
  }
}

int main(int,char**)
{
  PC_TEST_START
  test_net_buf();
  PC_TEST_END
  return 0;
}
