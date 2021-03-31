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
echo "6uiESo51K5kJDbXmT6SdJc9h2AtM9dmByFgpKky3M44M" > $KDIR/program_key.json
check "chmod 0400 $KDIR/program_key.json"
echo "GzWer3FtosUUso6TzG5wB4cchuXHfMWVvNu9R2FfaXH7" > $KDIR/mapping_key.json
check "chmod 0400 $KDIR/mapping_key.json"
