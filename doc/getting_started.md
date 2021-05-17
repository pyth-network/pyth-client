# Getting started with publishing via pyth-client

The pyth-client repo consists of a C++ library (libpc.a), a command-line administration tool (pyth) and a json/websocket-based server (pythd).

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

Once permissioned, you need two additional public keys in the key-store. The id of the mapping-account that contains the directory listing of the on-chain symbols and the id of the on-chain oracle program that you use to publish prices.  Mapping and program accounts are maintained in three separate environments: pythnet, devnet and forthcoming mainnet-beta.

Use the init_key_store.sh script to initialize these account keys:

```
KENV=pythnet  # or devnet or mainnet-beta

# initialize keys for solana devnet
../pctest/init_key_store.sh $KENV $KDIR

```

This creates two files: `$KDIR/program_key.json` and `$KDIR/mapping_key.json`.

Once permissioned, you can test your setup by running the test_publish.cpp example program for publishing and subscribing to two test symbols.  To test publishing on pythnet you need to run:


```
KHOST=44.232.27.44 # or devnet.solana.com
./test_publish -k $KDIR -r $KHOST
```

The certification environment (pythnet) can be found at the IP address 44.232.27.44. Please also provide to the administrator the IP address you plan to publish to pythnet from so that it can be added to the pythnet firewall.  Solana devnet can be found at: devnet.solana.com. No additional permissioning is required to publish to devnet.

You can also publish to solana using the pythd server. Start up the server using the same key-store directory and host specification:

```
./pythd -k $KDIR -r $KHOST
```

Run the test_publish.py example program on the same host to connect to the pythd server:

```
../pctest/test_publish.py

```

## Running the dashboard

The pythd server also exports a dashboard web page for watching the ticking pyth prices.  The public key you create above does not need publish-permission to watch the ticking pyth prices.  To activate the dashboard page include an additional `-w` argument to pythd pointing at the html/javscript code directory:

```
./pythd -k $KDIR -r $KHOST -w ../dashboard
```

Connect to the dashboard via http://localhost:8910.
