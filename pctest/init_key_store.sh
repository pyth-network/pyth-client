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
    MAP_KEY=BmA9Z6FjioHJPpjT39QazZyhDRUdZy2ezwx4GiDdE2u2
    PGM_KEY=gSbePebfvPy7tRqimPoVecS2UsBvYv46ynrzWocc92s
    PRM_KEY=AB6Br1PgMUiyy5tnTeBWwoqsZY3A5hxkpqa2TK8aNv6w
    ;;
  pythnet)
    MAP_KEY=Fy4NhY7n3yoWHZmVdEwsjjRt6tbHUgPpJ2j22ABVUy4B
    PGM_KEY=E7g6dJUeKaWJTyYPxKXcaLJhzkkLaK3NwPKNyzLfZSnP
    PRM_KEY=BcgWPzHix9F3GsSkbhx4aAxZ3aXPZCQCamxYQCUWeYto
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
echo $PRM_KEY > $KDIR/param_key.json
check "chmod 0400 $KDIR/param_key.json"
