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
  prodbeta)
    MAP_KEY=CUK2JnnLRMtJon3kAUXcgVc8wDhFk96HmNTssnaNRbXQ
    PGM_KEY=9SfxAUWNQKzx85jaivNhhUKG5JTZZmkFydjmdkKakjMo
    ;;
  devnet)
    MAP_KEY=BmA9Z6FjioHJPpjT39QazZyhDRUdZy2ezwx4GiDdE2u2
    PGM_KEY=gSbePebfvPy7tRqimPoVecS2UsBvYv46ynrzWocc92s
    ;;
  pythnet)
    MAP_KEY=Fy4NhY7n3yoWHZmVdEwsjjRt6tbHUgPpJ2j22ABVUy4B
    PGM_KEY=E7g6dJUeKaWJTyYPxKXcaLJhzkkLaK3NwPKNyzLfZSnP
    ;;
  *)
    echo "Unknown environment. Please use: prodbeta, devnet, pythnet"
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
check "chmod 0700 $KDIR"
