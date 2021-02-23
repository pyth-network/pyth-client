#include <pc/key_pair.hpp>
#include <pc/encode.hpp>
#include <pc/tx.hpp>
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
}

void test_tx()
{
  static const char snd[] = "[1,255,171,208,173,142,62,253,217,43,175,186,121,205,69,158,81,20,106,216,112,153,91,128,111,144,115,208,226,228,180,230,54,224,118,105,238,95,215,221,52,118,41,49,241,73,160,221,225,36,45,167,11,203,7,232,201,166,138,219,218,113,232,229 ]";
  static const char rcv[] = "[250,253,9,53,225,31,214,240,1,58,97,59,148,137,248,39,38,220,19,101,158,220,152,182,205,138,178,151,170,84,184,192,106,21,194,67,236,136,201,96,190,168,115,189,162,65,74,99,163,119,180,155,55,116,174,138,226,204,57,62,59,110,208,88]";
  static const char htxt[] = "GV8fmQm4Xo1dwhtuGQs1RnB1JH8fZM2sdkZD5NCegA3x";

  // send and receive key pairs
  key_pair ksnd, krcv;
  ksnd.init_from_json( snd );
  krcv.init_from_json( rcv );

  // receive public key
  pub_key prcv( krcv );

  // recent block hash
  hash rhash;
  rhash.dec_base58( (const uint8_t*)htxt, __builtin_strlen( htxt ) );

  // serialize transfer instruction
  uint8_t buf[1024];
  bincode msg;
  msg.attach( buf );
  long amt = 5000000000L; // 5 SOL
  tx::transfer( msg, rhash, ksnd, prcv, amt );

  // encode result
  static char restxt[] = "66vcTF2UeCrUG2JrP8igK8KnVnJYxc89w1fWmJDZChTfxAggim9Q3J8riCwtoEFMbL3awWAvR4HkRxLQ6jspD5JJawjPV1GqAojkK5zUjMzGHKujhnsMtyUpFKZp5tY7zkbebdRjbyDpHfQ4K4ujBzJAE1YSUuPUHskeoeWbdAofEpfUxRRqEGSj9dQLidGwd5yq1H5hihDBPg36iSVAw91o6s6uFvZPqN3VTW9Mrx4cVWJ4WXcdedg5aLWs98FZ3Fa5mxpXAWoZxbWH3bkaT3edxBwpLU78azzaj";
  char res[1024];
  int rlen = 1024;
  int elen = enc_base58( buf, msg.get_pos(), (uint8_t*)res, rlen );
  res[elen] = '\0';
  PC_TEST_CHECK( 0 == __builtin_strcmp( restxt, res ) );
}

int main(int,char**)
{
  PC_TEST_START
  test_key();
  test_tx();
  PC_TEST_END
  return 0;
}

