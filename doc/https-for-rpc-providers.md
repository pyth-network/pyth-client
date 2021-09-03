# HTTPS connections to RPC providers

The pyth-client code does not have native TLS support to connect to various RPC providers that require HTTPS connections in order to safeguard authentication credentials that a customer requires to access their services.

This lack will be remedied in a future release but in order to address the problem, we have provided some example `nginx` configurations for three providers as well as a wrapper configuration that will permit a developer or operations tech to run `nginx` locally without superuser privileges and on high numbered ports.  The configurations as provided expose the following ports and map to the following providers:

| Port number | Provider | protocol notes |
| ---- | ------------ | ------------------------------- |
| 7800 | Blockdaemon | websocket only |
| 7900 | Blockdaemon | HTTP RPC and websocket |
| 7801 | Figment | websocket only |
| 7901 | Figment | HTTP RPC and websocket |
| 7802 | TritonOne | websocket only |
| 7902 | TritonOne | HTTP RPC and websocket |

## Running `nginx` as a standalone, unprivileged process

1. Make sure `nginx` is installed on your machine, in your VM or, in your container.
2. Clone a copy of this repository or copy the `nginx-configs` directory to the machine in question.
3. `cd doc/nginx-configs`
4. In `standalone-dev-nginx.conf` replace `127.0.0.1` with the appropriate DNS resolver for your network, if necessary.  This can usually be retrived via `grep nameserver /etc/resolv.conf`.
5. For the RPC provider(s) which you are using, be sure to replace `YOUR_SERVER` and `REPLACE_ME_WITH_YOUR_AUTH_TOKEN` in the appropriate configuration file.
6. You can then run `nginx` as with the command `nginx -c $(pwd)/standalone-dev-nginx.conf -g 'daemon off;'`

An example follows.

```
[pyth@pyth-dev-vm nginx-configs]$ nginx -c $(pwd)/standalone-dev-nginx.conf -g 'daemon off;'
nginx: [alert] could not open error log file: open() "/var/log/nginx/error.log" failed (13: Permission denied)

^C
[pyth@pyth-dev-vm nginx-configs]$ 
```

There are a few caveats.

1. On most Linux distributions, the error shown above about `/var/log/nginx/error_log` failing to open is normal.  It's a hardcoded default for most builds.  It can be safely ignored.
2. The full, absolute path must be provided for the configuration file.  Any relative paths are assumed to be relative to `nginx`'s application data directory (usually `/usr/share/nginx/...`).

You can test any of the HTTP RPC interface for the providers with the following command line.  Replace `<PORT NUMBER>` with one of the port numbers in the table above.

`curl http://localhost:<PORT NUMBER> -X POST -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":1,"method":"getHealth"}'`

An example of the output expected:

```
[pyth@pyth-dev-vm nginx-configs]$ curl http://localhost:7901 -X POST -H 'Content-Type: application/json' -d '{"jsonrpc":"2.0","id":1,"method":"getHealth"}'
{"jsonrpc":"2.0","result":"ok","id":1}
[pyth@pyth-dev-vm nginx-configs]$ 
```

You can test any of the websocket interface for the providers with the following command line.  Replace `<PORT NUMBER>` with one of the port numbers in the table above.

`curl --include --no-buffer --header "Connection: Upgrade" --header "Upgrade: websocket" --header "Host: api.mainnet-beta.solana.com" --header "Origin: https://api.mainnet-beta.solana.com" --header "Sec-WebSocket-Key: abcdefghijklmnop==" --header "Sec-WebSocket-Version: 13" http://localhost:<PORT NUMBER>/`

An example to test the websocket interface:

```
[pyth@pyth-dev-vm nginx-configs]$ curl --include \
    --no-buffer \
    --header "Connection: Upgrade" \
    --header "Upgrade: websocket" \
    --header "Host: api.mainnet-beta.solana.com" \
    --header "Origin: https://api.mainnet-beta.solana.com" \
    --header "Sec-WebSocket-Key: abcdefghijklmnop==" \
    --header "Sec-WebSocket-Version: 13" \
    http://localhost:7801/
HTTP/1.1 101 Switching Protocols
upgrade: websocket
connection: Upgrade
sec-websocket-accept: FX/kNn6LL8gnqQea2uak6E6sPOU=

^C
[pyth@pyth-dev-vm nginx-configs]$ 
```

The virtual server configurations found in `nginx-configs/...` are meant as a sample to be used reference material.  Each `nginx` installation will require tweaking and fine tuning if it varies much from the software provided.
