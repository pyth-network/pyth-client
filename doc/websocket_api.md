# pyth-client websocket API
The pythd daemon supports a websocket interface based on the jsoni-rpc 2.0 standard

- get_symbol_list
- update_price
- subscribe_price
- subscribe_price_sched

A batch request containing both get_price and subscribe_price is guaranteed not to miss any intervening price updates.

Batch requests are processed in the order the requests appear within the batch.

## get_symbol_list

Get list of available symbols and associated meta data.

Request looks like:
```
{
  "jsonrpc": "2.0",
  "method": "get_symbol_list",
  "id" : 1
}
```

A successful response looks like:
```
{
  "jsonrpc": "2.0",
  "result" : [
    {
      "symbol" : "US.EQ.SYMBOL1",
      "price_exponent" : -4,
      "price_type" : "price"
    },
    {
      "symbol" : "US.EQ.SYMBOL2",
      "price_exponent" : -6,
      "price_type" : "price"
    }
  ],
  "id" : 1
}

```

## update_price

Update component price of some symbol using the publishing key of the pythd daemon.

Request looks like:
```
{
  "jsonrpc": "2.0",
  "method": "update_price",
  "params" : {
    "symbol": "US.EQ.SYMBOL1",
    "price_type": "price",
    "price" : 42002,
    "conf" : 3,
    "status": "trading"
  },
  "id" : 1
}
```

The params section is an array of length 5 containing (in order) the symbol,
price type, price,  confidence interval and status. The price and confidence interval is expressed as a fixed-point integer with an implied exponent defined in the symbol account. The price type is a string with one of the following values: "price" or "twap". The symbol status is a string with one of the following values: "trading" or "halted".

A successful response looks like:
```
{
  "jsonrpc": "2.0",
  "result" : 0,
  "id" : 1
}
```

# subscribe_price

Subscribe to symbol price updates.

Request looks like:

```
{
  "jsonrpc": "2.0",
  "method": "subscribe_price",
  "params" : {
    "symbol" : "US.EQ.SYMBOL1",
    "price_type" : "price"
  },
  "id" : 1
}
```

A successful response looks like:

```
{
  "jsonrpc": "2.0",
  "result" : {
    "subscription" : 1234
  },
  "id" : 1
}
```

Where the result is an integer corresponding to a subscription identifier. All subsequent notifications for this subscription correspond to this identifier.

```
{
  "jsonrpc": "2.0",
  "method": "notify_price",
  "params": {
    "result": {
      "price" : 42002,
      "conf" : 3,
      "status" : "trading",
      "valid_slot" : 32008,
      "pub_slot" : 32009
    },
    "subscription" : 1234
  }
}
```

Results include the most recent aggregate price, confidence interval and symbol status. pythd will submit a `notify_price` message immediately upon subscription with the latest price instead of waiting for an update.

## subscribe_price_sched

Subscribe to price update schedule. pythd will notify the client whenever it should submit the next price update for a subscribed symbol.

Request looks like:

```
{
  "jsonrpc": "2.0",
  "method": "subscribe_price_sched",
  "params" : {
    "symbol" : "US.EQ.SYMBOL1",
    "price_type" : "price"
  },
  "id" : 1
}
```

A successful response looks like:

```
{
  "jsonrpc": "2.0",
  "result" : {
    "subscription" : 1234
  },
  "id" : 1
}
```

Where the result is an integer corresponding to a subscription identifier. All subsequent notifications for this subscription correspond to this identifier.

```
{
  "jsonrpc": "2.0",
  "method": "notify_price_sched",
  "params": {
    "subscription" : 1234
  }
}
```


