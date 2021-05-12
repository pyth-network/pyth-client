#!/bin/bash

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

KENV=$1
KDIR=$2
case $KENV in
  devnet)
    MAP_KEY=ArppEFcsybCLE8CRtQJLQ9tLv2peGmQoKWFuiUWm4KBP
    PGM_KEY=5mkqGkkWSaSk2NL9p4XptwEQu4d5jFTJiurbbzdqYexF
    ;;
  pythnet)
    MAP_KEY=CpmU3oZqTWtPoFatbgpQAJNhmTq816Edp1xZpZAQiTUv
    PGM_KEY=FKVZxSxY4WnwLTgQQmzMoBF87hpsT2K4BPcMcT9u1YGD
    ;;
  *)
    echo "Unknown environment. Please use: devnet, pythnet"
    exit 1;
esac
if [ -z "$KDIR" ] ; then
  KDIR=$HOME/.pythd
fi

PKEY_FILE=$KDIR/publish_key_pair.json
if [ ! -f $PKEY_FILE ] ; then
  echo "cannot find $PKEY_FILE"
  exit 1
fi

echo $PGM_KEY > $KDIR/program_key.json
check "chmod 0400 $KDIR/program_key.json"
echo $MAP_KEY > $KDIR/mapping_key.json
check "chmod 0400 $KDIR/mapping_key.json"
