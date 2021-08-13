#!/bin/bash

PYTH=./pyth
SOLANA=../../solana/target/release/solana
SOLANA_KEYGEN=../../solana/target/release/solana-keygen
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

setup_pub()
{
  ADM=$1
  DIR=$2
  OPT="-k $DIR $RPC $FINAL"
  chmod 0700 $DIR

  # create publisher key and fund it
  check "$PYTH init_key $OPT"
  check "$SOLANA airdrop 10 -k $DIR/publish_key_pair.json $SOL_OPT"

  # get program public key
  prog_id=$($SOLANA_KEYGEN pubkey $ADM/program_key_pair.json)
  echo $prog_id > $DIR/program_key.json
  chmod 0400 $DIR/program_key.json

  # get mapping public key
  map_id=$($SOLANA_KEYGEN pubkey $ADM/mapping_key_pair.json)
  echo $map_id > $DIR/mapping_key.json
  chmod 0400 $DIR/mapping_key.json
}

ADIR=$1
PDIR=$2
if [ -z "$ADIR" ] ; then
  echo "setup_pub.sh <admin_key_store_dir> <publisher_key_store_dir>"
  exit 1
fi
if [ -z "$PDIR" ] ; then
  echo "setup_pub.sh <admin_key_store_dir> <publisher_key_store_dir>"
  exit 1
fi
setup_pub $ADIR $PDIR
