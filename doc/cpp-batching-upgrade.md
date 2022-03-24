# Batch Price Updates

As of 2.10.1, `pyth-client` has support for sending batched price updates. This significantly reduces SOL burn, so we strongly advise that you update to this newer version.

## Websocket API
If you are using the [websocket JRPC API](websocket_api.md) to send updates to `pythd`, all you need to do is update the version of pythd you are running to [`2.10.1`](https://github.com/pyth-network/pyth-client/releases/tag/mainnet-v2.10.1).

## C++ Bindings
If you are linking against the C++ bindings, you will need to make some changes to your code to support sending batched updates.

Instead of sending individual price updates in response to `price_sched` callbacks, you will now need to call `pc::price::send` with a vector of `pc::price` objects that you wish to publish, in response to `on_slot_publish` callbacks. These will then be batched into transactions. The [`test_publish.cpp`](../pctest/test_publish.cpp) example has been updated to use this new approach.

**Important**: this is a breaking change for C++ publishers, so you will need to update your code as described above for `2.10.1` to work.
