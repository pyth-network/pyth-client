#!/bin/bash

BIN=./pyth
KDIR=$1
if [ -z "$KDIR" ] ; then
  echo "test_agg.sh <key_store_directory>"
  exit 1
fi
TKEY=$(cat $KDIR/test_key.json)

TESTS=(
  test_qset_1.json
  test_qset_2.json
  test_qset_3.json
  test_qset_4.json
  test_qset_5.json
)
for i in ${TESTS[@]} ; do
  $BIN upd_test $TKEY ../pctest/$i -k $KDIR
done
