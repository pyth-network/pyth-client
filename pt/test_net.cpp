#include "test_error.hpp"
#include <pc/net_socket.hpp>
#include <pc/net_socket.hpp>
#include <iostream>

using namespace pc;

void test_net_buf()
{
  {
    net_wtr msg;
    msg.add( "hello" );
    PC_TEST_CHECK( msg.size() == 5 );
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
    msg.add( net_str( buf, 1460 ) );
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
    net_buf *hd, *tl;
    msg.detach( hd, tl );
    PC_TEST_CHECK( hd==tl );
    PC_TEST_CHECK( hd->next_ == nullptr );
    PC_TEST_CHECK( tl->size_ == 5 );
    PC_TEST_CHECK( 0 == __builtin_strncmp( "Hello", tl->buf_, 5 ) );
    hd->dealloc();
  }
  {
    net_wtr msg;
    msg.add( "one" );
    msg.add( "two" );
    msg.add( "three" );
    msg.add( '\0' );
    net_buf *hd, *tl;
    msg.detach( hd, tl );
    PC_TEST_CHECK( 0 == __builtin_strcmp( "onetwothree", tl->buf_ ) );
    hd->dealloc();
  }
}

void test_json_wtr()
{
  {
    json_wtr wtr;
    PC_TEST_CHECK( wtr.size() == 0UL );
    wtr.add_val( json_wtr::e_obj );
    wtr.pop();
    size_t wsize = wtr.size();
    net_buf *hd, *tl;
    wtr.detach(hd,tl);
    PC_TEST_CHECK( 0==__builtin_strncmp("{}",hd->buf_,wsize) );
    hd->dealloc();
  }
  {
    json_wtr wtr;
    wtr.add_val( json_wtr::e_obj );
    wtr.add_key( "foo", "hello" );
    wtr.add_key( "num", 42UL );
    wtr.add_key( "obj", json_wtr::e_obj );
    wtr.add_key( "bollox", "stuff" );
    wtr.pop();
    wtr.add_key( "arr", json_wtr::e_arr );
    wtr.add_val( 9328UL );
    wtr.add_val( "string" );
    wtr.add_val( json_wtr::e_obj );
    wtr.add_key( "k", "v" );
    wtr.pop();
    wtr.add_val( json_wtr::e_arr );
    wtr.add_val( "myval" );
    wtr.pop();
    wtr.pop();
    wtr.pop();
    const char *res = "{\"foo\":\"hello\",\"num\":42,\"obj\":{\"bollox\":\"stuff\"},\"arr\":[9328,\"string\",{\"k\":\"v\"},[\"myval\"]]}";
    size_t rlen = __builtin_strlen( res );
    size_t wsize = wtr.size();
    net_buf *hd, *tl;
    wtr.detach(hd,tl);
    PC_TEST_CHECK( rlen == wsize );
    PC_TEST_CHECK( rlen == hd->size_ );
    PC_TEST_CHECK( 0==__builtin_strncmp(res,hd->buf_,rlen ) );
    hd->dealloc();
  }
  {
    static const char kptxt[] = "[1,255,171,208,173,142,62,253,217,43,175,186,121,205,69,158,81,20,106,216,112,153,91,128,111,144,115,208,226,228,180,230,54,224,118,105,238,95,215,221,52,118,41,49,241,73,160,221,225,36,45,167,11,203,7,232,201,166,138,219,218,113,232,229]";
    key_pair kp;
    kp.init_from_json( kptxt );
    json_wtr wtr;
    wtr.add_val( kp );
    net_buf *hd, *tl;
    wtr.detach(hd,tl);
    PC_TEST_CHECK( __builtin_strlen( kptxt) == hd->size_ );
    PC_TEST_CHECK( 0==__builtin_strncmp( kptxt, hd->buf_, hd->size_ ));
    hd->dealloc();
  }
}

int main(int,char**)
{
  PC_TEST_START
  test_net_buf();
  test_json_wtr();
  PC_TEST_END
  return 0;
}
