#include <pc/key_pair.hpp>
#include <pc/misc.hpp>
#include <pc/log.hpp>
#include <pc/request.hpp>
#include "test_error.hpp"
#include <math.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <algorithm>

using namespace pc;

void test_key()
{
  // check key_pair encoding
  static const uint8_t kp_val[] = {
    1,255,171,208,173,142,62,253,217,43,175,186,121,205,69,158,81,20,106,216,112,153,91,128,111,144,115,208,226,228,180,230,54,224,118,105,238,95,215,221,52,118,41,49,241,73,160,221,225,36,45,167,11,203,7,232,201,166,138,219,218,113,232,229
  };
  static const char kptxt[] = "[1,255,171,208,173,142,62,253,217,43,175,186,121,205,69,158,81,20,106,216,112,153,91,128,111,144,115,208,226,228,180,230,54,224,118,105,238,95,215,221,52,118,41,49,241,73,160,221,225,36,45,167,11,203,7,232,201,166,138,219,218,113,232,229 ]";
  key_pair kp;
  kp.init_from_json( kptxt );
  for(unsigned i=0; i != key_pair::len; ++i ) {
    PC_TEST_CHECK( kp.data()[i] == kp_val[i] );
  }

  // check public key encoding
  static const char pktxt[] = "4hDXpxxchPLHUH4aCgr8Ec9B82Aztjy2w4xRc4NFhqCg";
  pub_key pk( kp );
  std::string res;
  pk.enc_base58( res );
  PC_TEST_CHECK( res == pktxt );

  pub_key pk2;
  pk2.init_from_text( str( pktxt ) );
  for(unsigned i=0; i != pub_key::len; ++i ) {
    PC_TEST_CHECK( pk.data()[i] == pk2.data()[i] );
  }

  // check message signing and encoding
  static const char sigtxt[] = "3LEWGZ5K88RqFnftjqyzaFm4AdYkwnGvJhKb13dVEa9uLnoDUif5B3esZyQ8dwxtx44PQZqkvhqH4HZUMi5PjTHQ";

  const char *msg = "hello world";
  size_t msglen = __builtin_strlen( msg );
  {
    std::string res;
    signature sig;
    PC_TEST_CHECK( sig.sign( (const uint8_t*)msg, msglen, kp ) );
    sig.enc_base58( res );
    PC_TEST_CHECK( res == sigtxt );
  }
  {
    signature sig;
    sig.init_from_text( sigtxt );
    PC_TEST_CHECK( sig.verify( (const uint8_t*)msg, msglen, pk ) );
  }

  // check key generation
  {
    key_pair gk;
    gk.gen();
    std::string res;
    signature sig;
    PC_TEST_CHECK( sig.sign( (const uint8_t*)msg, msglen, gk ) );
    pub_key gp( gk );
    PC_TEST_CHECK( sig.verify( (const uint8_t*)msg, msglen, gp ) );
  }

  // check sysvar ids
  str clock_var( "SysvarC1ock11111111111111111111111111111111" );
  union { uint8_t ckey[64]; uint64_t cid[8]; };
  PC_TEST_CHECK( 32 == dec_base58(
        (uint8_t*)clock_var.str_, clock_var.len_, ckey ) );
  uint64_t clock[] = { 0xc974c71817d5a706UL, 0xb65e1d6998635628UL,
    0x5c6d4b9ba3b85e8bUL, 0x215b5573UL };
  uint8_t buf[128];
  PC_TEST_CHECK( (int)clock_var.len_ ==
      enc_base58( (uint8_t*)clock, 32, buf, 128 ) );
  str cres( buf, clock_var.len_ );
  PC_TEST_CHECK( cres == clock_var );
}

void test_log()
{
  log::set_level( PC_LOG_DBG_LVL );
  PC_LOG_DBG( "example" )
    .add( "hello", str( "world" ) )
    .add( "ival", 42L )
    .add( "fval", 3.14159 )
    .end();
  log::set_level( PC_LOG_INF_LVL );
  PC_LOG_DBG( "example2" )
    .add( "hello", str( "world2") )
    .end();
  PC_LOG_INF( "example3" )
    .add( "hello", str( "world3" ))
    .end();
}

class test_request : public request
{
public:
  test_request( const std::string& val ) : val_( val ) {
  }
  void submit() {
    on_response_sub( this );
  }
  std::string val_;
};

class test_sub : public request_sub,
                 public request_sub_i<test_request>
{
public:
  test_sub() : id_(-1) {}
  void on_response( test_request *rptr, uint64_t val ) {
    val_ = rptr->val_;
    id_  = val;
  }
  bool check( const std::string& val, uint64_t id ) {
    return val == val_ && id == id_;
  }
  std::string val_;
  uint64_t    id_;
};

void test_request_sub()
{
  test_sub sub1, sub2;
  request_sub_set  psub1(&sub1);
  request_sub_set  psub2(&sub2);

  test_request r1("r1"), r2("r2"), r3("r3");
  uint64_t p1_1 = psub1.add( &r1 );
  uint64_t p1_2 = psub1.add( &r2 );
  uint64_t p1_3 = psub1.add( &r3 );
  uint64_t p2_1 = psub2.add( &r1 );
  uint64_t p2_2 = psub2.add( &r2 );
  uint64_t p2_3 = psub2.add( &r3 );

  // check subscription ids
  PC_TEST_CHECK( p1_1 != p1_2 );
  PC_TEST_CHECK( p1_2 != p1_3 );
  PC_TEST_CHECK( p1_1 != p1_3 );

  // check cross-subscription
  r1.submit();
  PC_TEST_CHECK( sub1.check( "r1", p1_1 ) );
  PC_TEST_CHECK( sub2.check( "r1", p2_1 ) );
  r2.submit();
  PC_TEST_CHECK( sub1.check( "r2", p1_2 ) );
  PC_TEST_CHECK( sub2.check( "r2", p2_2 ) );
  r3.submit();
  PC_TEST_CHECK( sub1.check( "r3", p1_3 ) );
  PC_TEST_CHECK( sub2.check( "r3", p2_3 ) );
  r2.submit();
  PC_TEST_CHECK( sub1.check( "r2", p1_2 ) );
  PC_TEST_CHECK( sub2.check( "r2", p2_2 ) );

  // remove request1 from subscriber 1
  psub1.del( p1_1 );
  r1.submit();
  PC_TEST_CHECK( sub1.check( "r2", p1_2 ) );
  PC_TEST_CHECK( sub2.check( "r1", p1_1 ) );

  // remove request3 from subscriber2
  psub2.del( p2_3 );
  r3.submit();
  PC_TEST_CHECK( sub1.check( "r3", p1_3 ) );
  PC_TEST_CHECK( sub2.check( "r1", p1_1 ) );

  // check subscription id reuse
  psub1.del( p1_3 );
  PC_TEST_CHECK( p1_3 == psub1.add( &r1 ) );
  r1.submit();
  PC_TEST_CHECK( sub1.check( "r1", p1_3 ) );
}

int main(int,char**)
{
  PC_TEST_START
  test_key();
  test_log();
  test_request_sub();
  PC_TEST_END
  return 0;
}
