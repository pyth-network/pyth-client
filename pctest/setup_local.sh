#!/bin/bash

PYTH=./pyth
SOLANA=../../solana/target/release/solana
SOL_OPT="-u localhost --commitment finalized"
RPC="-r localhost"
FINAL="-c finalized"

check()
{
  CMD=$1
  echo "$CMD"
  eval $CMD
  RC=$?
  if [ $RC -ne 0 ]; then
    echo "unexpected error executing rc= $RC"
    exit 1
  fi
}

setup_admin()
{
  DIR=$1
  OPT="-k $DIR $RPC $FINAL"
  chmod 0700 $DIR

  # create publisher key and fund it
  check "$PYTH init_key $OPT"
  check "$SOLANA airdrop 10 -k $DIR/publish_key_pair.json $SOL_OPT"

  # create program key
  check "$PYTH init_program $OPT"

  # setup program key and deploy
  check "$SOLANA program deploy ../target/oracle.so -k $DIR/publish_key_pair.json --program-id $DIR/program_key_pair.json $SOL_OPT"

  # setup mapping account
  check "$PYTH init_mapping $OPT"

  # setup test account
  check "$PYTH init_test $OPT"
}

# create key_store directory and initialize key, program and mapping
# accounts assuming you have a local build of solana and are running
# both a solana-validator and solana-faucet on localhost
# run from build/ directory
KDIR=$1
if [ -z "$KDIR" ] ; then
  echo "setup_local.sh <key_store_directory>"
  exit 1
fi
setup_admin $KDIR
