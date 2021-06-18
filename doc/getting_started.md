# Getting started with publishing via pyth-client

The pyth-client repo consists of a C++ library (libpc.a), a command-line administration tool (pyth), a json/websocket-based server (pythd) and a gateway/proxy server for price publishing (pyth_tx).

You can integrate with libpc directly in your application. See `pctest/test_publish.cpp` for an example. Or, you can integrate with the pythd server via json/websockets. See `pctest/test_publish.py` for an example.

Before doing this you need to setup a *key-store* directory. A key-store is a collection of cryptographic keys for interracting with the solana block-chain. This can be done via the pyth command-line tool.  First, build the project by following the instructions in the README.md file and cd to the build directory, then run the following:


```
KDIR=$HOME/.pythd
./pyth init_key -k $KDIR

```

You can replace `$KDIR` with a directory of your choice.

This creates the directory (if it didnt already exist) and creates a key-pair file containing the identifier you're going to use to publish to the block-chain: `$KDIR/publish_key_pair.json`.

Please extract the public key from this key-pair and send it to the administrator so it can be permissioned for your symbols of interest. The public key can be extracted as follows:

```
./pyth get_pub_key $KDIR/publish_key_pair.json
```

This will output the public key in base58 encoding and will look something like:

```
5rYvdyWAunZgD2EC1aKo7hQbutUUnkt7bBFM6xNq2z7Z
```

Once permissioned, you need two additional public keys in the key-store. The id of the mapping-account that contains the directory listing of the on-chain symbols and the id of the on-chain oracle program that you use to publish prices.  Mapping and program accounts are maintained in two separate environments: devnet and forthcoming mainnet-beta.

Use the init_key_store.sh script to initialize these account keys:

```
KENV=devnet  # or mainnet-beta

# initialize keys for solana devnet
../pctest/init_key_store.sh $KENV $KDIR

```

This creates two files: `$KDIR/mapping_key.json` and `$KDIR/program_key.json`.

Once permissioned, you can test your setup by running the test_publish.cpp example program for publishing and subscribing to two test symbols.  To test publishing on devnet you need to run:


```
KHOST=api.devnet.solana.com
THOST=<your_pyth_tx_host>
./test_publish -k $KDIR -r $KHOST -t $THOST
```

The -r and -t options correspond to the locations of required solana validator and pyth_tx server instances.

You can also publish to solana using the pythd server. Start up the server using the same key-store directory and host specification:

```
./pythd -k $KDIR -r $KHOST -t $THOST
```

Run the test_publish.py example program on the same host to connect to the pythd server:

```
../pctest/test_publish.py

```

## Running the pyth_tx server

The pyth-client API connects to two separate services via TCP. These include the solana validator and pyth_tx servers. The pyth_tx server is included in the pyth-client repository.

The solana rpc interface is used for tracking slot and account updates. The pyth_tx server is used to submit price transactions directly to solana leader nodes via UDP.

pyth_tx runs as a separate server rather than being integrated directly into the client library to avoid a publisher from having to submit UDP datagrams to hundreds of different IP address from within an organization's firewall. Instead, a pyth_tx server can be deployed outside the firewall and a publisher clienti, running inside the firewall, can make a single TCP connection to it on a dedicated port.

You can run your own pyth_tx instance (recommended) or connect to provided server instances running against mainnet-beta.

Start up the pyth_tx server as follows:

```
./pyth_tx -r $KHOST
```

The -r option points to the IP address of a solana validator node or rpc end-point. It can be the same or different IP address to that passed to test_publish or pythd as long as it corresponds to the same environment.


## Running the dashboard

The pythd server also exports a dashboard web page for watching the ticking pyth prices.  The public key you create above does not need publish-permission to watch the ticking pyth prices.  To activate the dashboard page include an additional `-w` argument to pythd pointing at the html/javscript code directory:

```
./pythd -k $KDIR -r $KHOST -w ../dashboard
```

Connect to the dashboard via http://localhost:8910.
