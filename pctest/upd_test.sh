#!/bin/bash

BIN=./pyth
TESTS=(
  test_qset_1.json
  test_qset_2.json
  test_qset_3.json
  test_qset_4.json
)
for i in ${TESTS[@]} ; do
  $BIN upd_test ../pctest/$i -k ~/prm_admin
done
