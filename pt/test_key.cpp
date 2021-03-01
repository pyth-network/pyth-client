#include <pc/key_pair.hpp>
#include <pc/misc.hpp>
#include <pc/log.hpp>
#include "test_error.hpp"
#include <iostream>
#include <vector>
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
  pk2.init_from_text( pktxt );
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
}

void test_log()
{
  log::set_level( PC_LOG_DBG_LVL );
  PC_LOG_DBG( "example" )
    .add( "hello", "world" )
    .add( "ival", 42L )
    .add( "fval", 3.14159 )
    .end();
  log::set_level( PC_LOG_INF_LVL );
  PC_LOG_DBG( "example2" )
    .add( "hello", "world2" )
    .end();
  PC_LOG_INF( "example3" )
    .add( "hello", "world3" )
    .end();
}

int main(int,char**)
{
  PC_TEST_START
  test_key();
  test_log();
  PC_TEST_END
  return 0;
}

