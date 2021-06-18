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
  test_qset_6.json
  test_qset_7.json
  test_qset_8.json
  test_qset_9.json
  test_qset_10.json
  test_qset_11.json
  test_qset_12.json
  test_qset_13.json
  test_qset_14.json
  test_qset_15.json
  test_qset_16.json
  test_qset_17.json
  test_qset_18.json
  test_qset_19.json
  test_qset_20.json
  test_qset_21.json
  test_qset_22.json
  test_qset_23.json
  test_qset_24.json
  test_qset_25.json
  test_qset_26.json
  test_qset_27.json
  test_qset_28.json
  test_qset_29.json
  test_qset_30.json
)
for i in ${TESTS[@]} ; do
  $BIN upd_test $TKEY ../pctest/$i -k $KDIR
done
