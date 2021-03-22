# Getting started with publishing via pyth-client

The pyth-client repo consists of a C++ library (libpc.a), a command-line administration tool (pyth) and a json/websocket-based server (pythd).

You can integrate with libpc directly in your application (see `pctest/test_publish.cpp` example) or connect to the pythd server and interact via json-based messaging over websockets (see `pctest/test_publish.py`).

Before doing this you need to setup a *key-store* directory. A key-store is a collection of cryptographic keys for interracting with the solana block-chain. This can be done via the pyth command-line tool.  First, build the project by following the instructions in the README.md file and cd to the build directory, then run the following:


```
KDIR=$HOME/.pythd
./pyth init_key -k $KDIR

```

You can replace `$KDIR` with a directory of your choice.

This creates the directory (if it didnt already exist) and creates a key-pair file containing the identifier you're going to use to publish to the block-chain: `$KDIR/publish_key_pair.json`.

Please extract the public key from this key-pair and send it to the administrator so it can be permissioned for your symbols of interest. The public key can be extracted as follows:

```
./pyth pub_key $KDIR/publish_key_pair.json
```

This will output the public key in base58 encoding and will look something like:

```
5rYvdyWAunZgD2EC1aKo7hQbutUUnkt7bBFM6xNq2z7Z
```

Once permissioned, you need two additional public keys in the key-store. The id of the so-called mapping-account that contains the list of on-chain symbols and the id of the on-chain oracle program that you use to publish prices.  A program already exists to initialize these for testing on solana devnet:

```
../pctest/init_key_store.sh $KDIR
```

This creates two files: `$KDIR/program_key.json` and `$KDIR/mapping_key.json`.

Once pemissioned, you can test your setup by running the test_publisher.cpp example program for publishing and subscribing to two symbols on the devnet solana chain:

```
./test_publish -k $KDIR -r devnet.solana.com
```

There is a python version of the same program that uses the pythd server instead to publish prices on-chain:

```
# start the pythd server
./pythd -k $KDIR -r devnet.solana.com &

# start the publisher
../pctest/test_publish.py
```
