#!/usr/bin/env bash
#

# While Solana doesn't support resizing programs, the oracle binary needs to be smaller than 81760 bytes
# (The available space for the oracle program on pythnet is 88429 and mainnet is 81760)
ORACLE_SIZE=$(wc -c ./target/deploy/pyth_oracle.so | awk '{print $1}')
if [ $ORACLE_SIZE -lt 88429 ]
then
    echo "Size of pyth_oracle.so is small enough to be deployed to mainnet."
    echo $ORACLE_SIZE
else
    echo "Size of pyth_oracle.so is too big to be deployed to mainnet."
    echo $ORACLE_SIZE
    exit 1
fi