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
echo "7LmwnZDXi2d9dYsp45WsPkudU2Gb2ftjUC6fsxYvZocu" > $KDIR/program_key.json
check "chmod 0400 $KDIR/program_key.json"
echo "HQ9xBAuxpXcd9BgNjgE1Jm4uhwvU8BicP5w3gKfPRuAo" > $KDIR/mapping_key.json
check "chmod 0400 $KDIR/mapping_key.json"
