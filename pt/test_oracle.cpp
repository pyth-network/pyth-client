#include <program/src/oracle/oracle.h>
#include <pc/key_pair.hpp>
#include "test_error.hpp"

void add_symbol( pc_symbol_t *sptr, const char *sym )
{
  int len = std::min( PC_SYMBOL_SIZE, (int)__builtin_strlen( sym ) );
  __builtin_memset( &sptr->k1_, 0, sizeof( pc_symbol_t ) );
  __builtin_memcpy( &sptr->k1_, sym, len );
}

void add_key( pc_pub_key_t *aptr, const pc::pub_key& p )
{
  __builtin_memcpy( aptr->k1_, p.data(), pc::pub_key::len );
}

void test_map_table()
{
  pc::key_pair pp1, pp2;
  pp1.gen();
  pp2.gen();
  pc::pub_key  pk1(pp1), pk2(pp2);
  const char *sym1 = "nyse.TSLA";
  const char *sym2 = "nyse.TSLA2";
  const char *sym3 = "foo";

  pc_map_table_t tab[1];
  __builtin_memset( tab, 0, sizeof( tab ) );

  pc_symbol_t s1[1], s2[1], s3[1];
  pc_pub_key  p1[1], p2[2];
  add_symbol( s1, sym1 );
  add_symbol( s2, sym2 );
  add_symbol( s3, sym3 );
  add_key( p1, pk1 );
  add_key( p2, pk2 );
  PC_TEST_CHECK( PC_RETURN_SUCCESS ==
     pc_map_table_add_symbol( tab, s1, p1, 4 ) );
  PC_TEST_CHECK( PC_RETURN_SYMBOL_EXISTS ==
     pc_map_table_add_symbol( tab, s1, p2, 4 ) );
  PC_TEST_CHECK( PC_RETURN_SUCCESS ==
     pc_map_table_add_symbol( tab, s2, p2, 6 ) );

  pc_pub_key_t *res = NULL;
  res = pc_map_table_get_account( tab, s1 );
  PC_TEST_CHECK( res!= NULL && pc_pub_key_equal( res, p1 ) );
  res = pc_map_table_get_account( tab, s2 );
  PC_TEST_CHECK( res!=NULL && pc_pub_key_equal( res, p2 ) );
  res = pc_map_table_get_account( tab, s3 );
  PC_TEST_CHECK( res == NULL );
}

int main(int,char**)
{
  PC_TEST_START
  test_map_table();
  PC_TEST_END
}
