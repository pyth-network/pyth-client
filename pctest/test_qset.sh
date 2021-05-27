#!/bin/bash

BIN=./test_qset
TESTS=(
  test_qset_1.json
  test_qset_2.json
  test_qset_3.json
  test_qset_4.json
  test_qset_5.json
)
for i in ${TESTS[@]} ; do
  $BIN ../pctest/$i
done
