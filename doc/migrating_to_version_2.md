# Migrating to pyth version 2

Version 2 of the pyth on-chain program contains the full implementation of the aggregate price and confidence interval calculation. There have been changes to the price account data structure to make room for additional outputs including twap and volatility. A new account, known as the parameter account, has been introduced and contains math lookup tables that are required to compute the aggregate price.

A new set of mapping, product, price and program accounts has been deployed to devnet for version 2 and all publishers and subscribers should to migrate to these new accounts.

## Migrating publishers

Publishers should upgrade to the latest version of the pyth-client API (version 2).  Two git branches, corresponding to versions 1 and 2, labelled v1 and v2, have been established to facilitate this.

Publishers need to migrate to new account keys.  You should preserve your existing publishing key but update the mapping_key.json, program_key.json and param_key.json files in your key store directory. The simplest way to do this is to create a new directory, copy your publish_key_pair.json file to that directory and then run init_key_store.sh to generate the new account keys:

```
KENV=pythnet  # or devnet or mainnet-beta
# KDIR= old key-store directory
# NDIR= new key-store directory

# copy publish key over to new directory
mkdir $NDIR
cp $KDIR/publish_key_pair.json $NDIR

# initialize new directory
../pctest/init_key_store.sh $KENV $NDIR
```

The client API is also versioned and will fail to run against on-chain accounts that are set to a different version, so version 1 accounts must run against the v1 branch of the software and version 2 accounts must run against the v2 branch of the software.

## Migrating subscribers

On-chain subscribers should migrate to the new accounts defined above in the publishers section.  New price accounts contain additional values derived from the aggregate price including twap and volatility as well as space for additional contributing publishers, now upgraded to 32 publishers from 16 in version 1:

```
// from program/src/oracle/oracle.h:
typedef struct pc_price
{
//...
  int64_t         twap_;              // time-weighted average price
  int64_t         avol_;              // annualized price volatility
//....
  pc_price_comp_t comp_[PC_COMP_SIZE];// component prices
} pc_price_t;

```

Please note that the version field in all version 2 account headers now read 2 instead of 1.
