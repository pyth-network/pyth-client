# pyth-client websocket API
The pythd daemon supports a websocket interface based on the json-rpc 2.0 standard.  Methods include:

- [get_product_list](#get_product_list)
- [update_price](#update_price)
- [subscribe_price](#subscribe_price)
- [subscribe_price_sched](#subscribe_price_sched)
- [get_product](#get_product)
- [get_all_products](#get_all_products)

Batch requests are processed in the order the requests appear within the batch.

## get_product_list

Get list of available symbols and associated meta data.

Request looks like:
```
{
  "jsonrpc": "2.0",
  "method": "get_product_list",
  "id" : 1
}
```

A successful response looks something like:
```
{
 "jsonrpc": "2.0",
 "result": [
  {
   "account": "9F6eBgAfktth93C9zmtKDXFXNjZkq6JwJR56VPKqWmJm",
   "attr_dict": {
    "symbol": "SYMBOL1/USD",
    "asset_type": "Equity",
    "country": "USA",
    "description": "pyth example product #1",
    "quote_currency": "USD",
    "tenor": "Spot",
    "cms_symbol": "SYMBOL1",
    "cqs_symbol": "SYMBOL1",
    "nasdaq_symbol": "SYMBOL1"
   },
   "price": [
    {
     "account": "CrZCEEt3awgkGLnVbsv45Pp4aLhr7fZfZr3ubzrbNXaq",
     "price_exponent": -4,
     "price_type": "price"
    }
   ]
  },
  {
   "account": "HCFaDYyz1ajS57HfCaaqzA1cZSaa2oEccQejbHaaofd4",
   "attr_dict": {
    "symbol": "SYMBOL2/USD",
    "asset_type": "Equity",
    "country": "USA",
    "description": "pyth example product #2",
    "quote_currency": "USD",
    "tenor": "Spot",
    "cms_symbol": "SYMBOL2",
    "cqs_symbol": "SYMBOL2",
    "nasdaq_symbol": "SYMBOL2"
   },
   "price": [
    {
     "account": "7FUsKvvtN5rB1fgYFWZLo5DLcqHTTeu63bUPThYT6MiS",
     "price_exponent": -4,
     "price_type": "price"
    }
   ]
  }
 ],
 "id": null
}
```

## update_price

Update component price of some symbol using the publishing key of the pythd daemon.

Request includes the pricing account from the get_product_list output and looks something like:
```
{
  "jsonrpc": "2.0",
  "method": "update_price",
  "params" : {
    "account": "CrZCEEt3awgkGLnVbsv45Pp4aLhr7fZfZr3ubzrbNXaq",
    "price" : 42002,
    "conf" : 3,
    "status": "trading"
  },
  "id" : 1
}
```

The price and confidence interval (conf) attributes are expressed as integers with an implied decimal point given by the the price_exponent defined by symbol. The price type is a string with one of the following values: "price" or "twap". The symbol status is a string with one of the following values: "trading" or "halted".

A successful response looks like:
```
{
  "jsonrpc": "2.0",
  "result" : 0,
  "id" : 1
}
```

## subscribe_price

Subscribe to symbol price updates.

Request looks like:

```
{
  "jsonrpc": "2.0",
  "method": "subscribe_price",
  "params" : {
    "account": "CrZCEEt3awgkGLnVbsv45Pp4aLhr7fZfZr3ubzrbNXaq",
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

The subscription identifier in the result is used in all subsequent notifications for this subscription. An example notification (with the same subscription id) looks like:


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

Results also include two slot numbers. `valid_slot` corresponds to the slot containing the prices that were used to compute the aggregate price. `pub_slot` corresponds to the slot in which the aggregation price was published.

## subscribe_price_sched

Subscribe to price update schedule. pythd will notify the client whenever it should submit the next price for a subscribed symbol.

Request looks like:

```
{
  "jsonrpc": "2.0",
  "method": "subscribe_price_sched",
  "params" : {
    "account": "CrZCEEt3awgkGLnVbsv45Pp4aLhr7fZfZr3ubzrbNXaq",
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

## get_product

Get full set of data for the given product.

Request looks like:
```
{
  "jsonrpc": "2.0",
  "method": "get_product",
  "params": {
    "account": "4aDoSXJ5o3AuvL7QFeR6h44jALQfTmUUCTVGDD6aoJTM"
  },
  "id" : 1
}
```

A successful response looks something like:
```
{
  "jsonrpc": "2.0",
  "result": {
    "account": "4aDoSXJ5o3AuvL7QFeR6h44jALQfTmUUCTVGDD6aoJTM",
    "attr_dict": {
      "asset_type": "Crypto",
      "symbol": "BTC/USD",
      "country": "US",
      "quote_currency": "USD",
      "description": "BTC/USD",
      "tenor": "Spot",
      "generic_symbol": "BTCUSD"
    },
    "price_accounts": [
      {
        "account": "GVXRSBjFk6e6J3NbVPXohDJetcTjaeeuykUpbQF8UoMU",
        "price_type": "price",
        "price_exponent": -8,
        "status": "trading",
        "price": 4426101900000,
        "conf": 4271150000,
        "twap": 4433467600000,
        "twac": 1304202670,
        "valid_slot": 91402257,
        "pub_slot": 91402259,
        "prev_slot": 91402256,
        "prev_price": 4425895500000,
        "prev_conf": 3315350000,
        "publisher_accounts": [
          {
            "account": "HekM1hBawXQu6wK6Ah1yw1YXXeMUDD2bfCHEzo25vnEB",
            "status": "trading",
            "price": 4426958500000,
            "conf": 1492500000,
            "slot": 91402255
          },
          {
            "account": "GKNcUmNacSJo4S2Kq3DuYRYRGw3sNUfJ4tyqd198t6vQ",
            "status": "trading",
            "price": 4424690000000,
            "conf": 3690000000,
            "slot": 91402256
          }
        ]
      }
    ]
  },
  "id": 1
}
```

## get_all_products

Get full set of data for all products.

Request looks like:
```
{
  "jsonrpc": "2.0",
  "method": "get_all_products",
  "id" : 1
}
```

A successful response looks something like:
```
{
  "jsonrpc": "2.0",
  "result": [
    {
      "account": "5uKdRzB3FzdmwyCHrqSGq4u2URja617jqtKkM71BVrkw",
      "attr_dict": {
        "asset_type": "Crypto",
        "symbol": "BCH/USD",
        "country": "US",
        "quote_currency": "USD",
        "description": "BCH/USD",
        "tenor": "Spot",
        "generic_symbol": "BCHUSD"
      },
      "price_accounts": [
        {
          "account": "5ALDzwcRJfSyGdGyhP3kP628aqBNHZzLuVww7o9kdspe",
          "price_type": "price",
          "price_exponent": -8,
          "status": "trading",
          "price": 60282000000,
          "conf": 26000000,
          "twap": 60321475000,
          "twac": 22504746,
          "valid_slot": 91402601,
          "pub_slot": 91402604,
          "prev_slot": 91402600,
          "prev_price": 60282000000,
          "prev_conf": 26000000,
          "publisher_accounts": [
            {
              "account": "HekM1hBawXQu6wK6Ah1yw1YXXeMUDD2bfCHEzo25vnEB",
              "status": "trading",
              "price": 60282000000,
              "conf": 26000000,
              "slot": 91402599
            },
            {
              "account": "2V7t5NaKY7aGkwytCWQgvUYZfEr9XMwNChhJEakTExk6",
              "status": "unknown",
              "price": 0,
              "conf": 0,
              "slot": 0
            }
          ]
        }
      ]
    },
    {
      "account": "3nuELNFBkbXqsXtnCzphRPCX6toKKYxVDnkyr9pTwB1K",
      "attr_dict": {
        "asset_type": "Crypto",
        "symbol": "SABER/USD",
        "country": "US",
        "quote_currency": "USD",
        "description": "SABER/USD",
        "tenor": "Spot",
        "generic_symbol": "SABERUSD"
      },
      "price_accounts": [
        {
          "account": "8Td9VML1nHxQK6M8VVyzsHo32D7VBk72jSpa9U861z2A",
          "price_type": "price",
          "price_exponent": -8,
          "status": "trading",
          "price": 5785000,
          "conf": 5000,
          "twap": 5856365,
          "twac": 10241,
          "valid_slot": 91402601,
          "pub_slot": 91402604,
          "prev_slot": 91402600,
          "prev_price": 5785000,
          "prev_conf": 5000,
          "publisher_accounts": [
            {
              "account": "GKNcUmNacSJo4S2Kq3DuYRYRGw3sNUfJ4tyqd198t6vQ",
              "status": "trading",
              "price": 5785000,
              "conf": 5000,
              "slot": 91402601
            }
          ]
        }
      ]
    }
  ],
  "id": 1
}
```
