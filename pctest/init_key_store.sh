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
echo "3CwnV3MYXXoZNoX7XKrCJ9AJNTH2AqqjqRzVqYAYtMu8" > $KDIR/program_key.json
check "chmod 0400 $KDIR/program_key.json"
echo "5pNEzo7EKZPVkgGy3NPicfiGDq5j1QYYrurjKykHNf7z" > $KDIR/mapping_key.json
check "chmod 0400 $KDIR/mapping_key.json"
