#!/usr/bin/env bash
#
set -e

max_size=$1
filename=${2:-./target/deploy/pyth_oracle.so}

# While Solana doesn't support resizing programs, the oracle binary needs to be smaller than 81760 bytes
# (The available space for the oracle program on pythnet is 88429 and mainnet is 81760)
ORACLE_SIZE=$(wc -c $filename | awk '{print $1}')
if [ $ORACLE_SIZE -lt ${max_size} ]
then
    echo "Size of ${filename} is small enough to be deployed, since ${ORACLE_SIZE} is less than ${max_size}"
else
    echo "Size of ${filename} is too big to be deployed, since ${ORACLE_SIZE} is greater than ${max_size}"
    exit 1
fi
