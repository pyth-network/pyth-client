#!/bin/bash

PYTH=./pyth
PYTHD=./pythd
SOLANA=../../solana/target/release/solana
#SOL_OPT="-u d --commitment finalized"
#RPC="-r devnet.solana.com"
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

not_check()
{
  CMD=$1
  echo "$CMD"
  eval $CMD
  RC=$?
  if [ $RC -eq 0 ]; then
    echo "unexpected success executing rc= $RC"
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
  check "$SOLANA airdrop 5 -k $DIR/publish_key_pair.json $SOL_OPT"

  # create program key
  check "$PYTH init_program $OPT"

  # setup program key and deploy
  check "$SOLANA program deploy ../target/oracle.so -k $DIR/publish_key_pair.json --program-id $DIR/program_key_pair.json $SOL_OPT"

  # setup mapping account
  check "$PYTH init_mapping 0.5 $OPT"
}

setup_pub()
{
  ADM=$1
  DIR=$2
  OPT="-k $DIR $RPC $FINAL"
  chmod 0700 $DIR

  # create publisher key and fund it
  check "$PYTH init_key $OPT"
  check "$SOLANA airdrop 5 -k $DIR/publish_key_pair.json $SOL_OPT"

  # get program public key
  prog_id=$($PYTH get_pub_key $ADM/program_key_pair.json)
  echo $prog_id > $DIR/program_key.json
  chmod 0400 $DIR/program_key.json

  # get mapping public key
  map_id=$($PYTH get_pub_key $ADM/mapping_key_pair.json)
  echo $map_id > $DIR/mapping_key.json
  chmod 0400 $DIR/mapping_key.json
}

wait_log()
{
  LOG=$1
  while true; do
    if [ -f $LOG ]; then
      RC=`grep "completed_mapping_init" $LOG|wc -l`
      if [ $RC == "1" ]; then
        break;
      fi
      sleep .1
    fi
  done
}

#dirA=/home/richard/kadmin
#dirP=/home/richard/kpub
dirA=$(mktemp -d)
dirP=$(mktemp -d)
popt="$RPC $FINAL"
echo "dirA=$dirA"
echo "dirP=$dirP"
setup_admin $dirA
setup_pub $dirA $dirP

# try to add symbols from pub account
not_check "yes|$PYTH add_symbol US.EQ.SYMBOL1 price 5 -e -4 -k $dirP $popt"

# copy mapping key by not right key
cp $dirP/publish_key_pair.json $dirP/mapping_key_pair.json
not_check "yes|$PYTH add_symbol US.EQ.SYMBOL1 price 5 -e -4 -k $dirP $popt"
rm -f $dirP/mapping_key_pair.json

# create from mapping account
check "yes|$PYTH add_symbol US.EQ.SYMBOL1 price 0.1 -e -4 -k $dirA $popt"

# cant create twice
not_check "yes|$PYTH add_symbol US.EQ.SYMBOL1 price 0.1 -e -4 -k $dirA $popt"

# still cant create from publishing account
not_check "yes|$PYTH add_symbol US.EQ.SYMBOL1 price 0.2 -e -4 -k $dirP $popt"

# add some other symbols
check "yes|$PYTH add_symbol US.EQ.SYMBOL2 price 0.1 -e -6 -k $dirA $popt"
check "yes|$PYTH add_symbol US.EQ.SYMBOL3 price 0.05 -e -2 -k $dirA $popt"

# add publishers
pubA=$($PYTH get_pub_key $dirA/publish_key_pair.json)
check "yes|$PYTH add_publisher $pubA US.EQ.SYMBOL1 price -k $dirA $popt"
# check that we cant add same publisher twice
not_check "yes|$PYTH add_publisher $pubA US.EQ.SYMBOL1 price -k $dirA $popt"
check "yes|$PYTH add_publisher $pubA US.EQ.SYMBOL2 price -k $dirA $popt"
check "yes|$PYTH add_publisher $pubA US.EQ.SYMBOL3 price -k $dirA $popt"
# check that we cant add publisher for non-existent symbol
not_check "yes|$PYTH add_publisher $pubA US.EQ.SYMBOL4 price -k $dirA $popt"
pubP=$($PYTH get_pub_key $dirP/publish_key_pair.json)
not_check "yes|$PYTH add_publisher $pubP US.EQ.SYMBOL1 price -k $dirP $popt"
check "yes|$PYTH add_publisher $pubP US.EQ.SYMBOL1 price -k $dirA $popt"
check "yes|$PYTH add_publisher $pubP US.EQ.SYMBOL2 price -k $dirA $popt"
# no publisher for symbol3

# start pythd
echo "starting pythd daemons"
$PYTHD -k $dirA -p 8910 $RPC > $dirA/pythd.log 2>&1 &
$PYTHD -k $dirP -p 8911 $RPC > $dirP/pythd.log 2>&1 &
wait_log "$dirA/pythd.log"
wait_log "$dirP/pythd.log"

# run websocket integration tests
echo "running websocket tests"
../pctest/test_pyth.py
RC=$?

# cleanup
kill %1
kill %2
wait

# check if websocket tests succeeded
if [ $RC -ne 0 ]; then
  echo "unexpected error in websocket tests rc= $RC"
  exit 1
fi

# clean up temp directories
rm -fr $dirA
rm -fr $dirB
