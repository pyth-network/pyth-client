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

KDIR=$1
if [ -z "$KDIR" ] ; then
  KDIR=$HOME/.pythd
fi
echo "25d2dsoksoMjjzKJ7tyP4n1CWLzmb4MTHhqJEbzGbT8q" > $KDIR/program_key.json
check "chmod 0400 $KDIR/program_key.json"
echo "B2Csr2HUoq7CPCnjXBuxD7jb4cbpsQ2nFY9kSy61MUkN" > $KDIR/mapping_key.json
check "chmod 0400 $KDIR/mapping_key.json"
