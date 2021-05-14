# Aggregate price on-chain account structure

The pyth block-chain price oracle manages a number of on-chain accounts.  Accounts are segregated into different types corresponding to product reference data, price/quote data and directory, or mapping data. Mapping accounts serve as a listing or mapping of other accounts.

The relationships between different account types on-chain are as follows:

```

  -------------        -------------       -------------
  |           |1      m|           |       |           |
  |  mapping  |------->|  product  |------>|   price   |
  |           |        |           |       |  (price)  |
  -------------        -------------       -------------
        |                                        |
        V                                        V
  -------------                            -------------
  |           |                            |           |
  |  mapping  |                            |   price   |
  |           |                            |   (twap)  |
  -------------                            -------------
        |                                        |
        V                                        V
       ...                                      ...

```

There exists a linked list of `mapping` accounts. Each contains a simple list of `product` account ids, or public keys. Each `product` contains a set of reference attributes corresponding to a single product or asset, such as BTC/USD or AAPL/USD.  These attributes are stored as a list of text key/value pairs such as "symbol", "asset_type", "country" etc..

Each `product` account refers to the head of a linked-list of `price` accounts.  `price` accounts are where a product's price and other metrics are stored.  Each `price` account corresponds to a different "price_type". At the moment, these include "price", "twap" and "volatility".


## rust example

The rust example (pcrust/ or crates.io package pyth-client v0.1.1 ) program uses the solana rust RPC API to load and print all pyth product and price data by bootstrapping from the root `mapping` account key. The output looks something like this:

```
product_account .. 4SxmcsbJWVBWvuP2cRQDjFtAgdqzWWLbHESnUTH4CegT
  symbol.......... AAPL/USD
  asset_type...... Equity
  country......... US
  quote_currency.. USD
  tenor........... Spot
  description..... APPLE INC
  cms_symbol...... AAPL
  cqs_symbol...... AAPL
  nasdaq_symbol... AAPL
  price_account .. CeLD8TC7Ktv2o57EHP7rddh6TZBHXkAhTyfWgxsTYs21
    price_type ... price
    exponent ..... -5
    status ....... trading
    corp_act ..... nocorpact
    price ........ 12276250
    conf ......... 1500
    valid_slot ... 55519683
    publish_slot . 55519684
product_account .. 9BQ2WKSVbfzpSJYyLCJuxVuiUFxHmQvkhhez94AdffEZ
  symbol.......... BTC/USD
  asset_type...... Crypto
  country......... US
  quote_currency.. USD
  tenor........... Spot
  jlqd_symbol..... BTCUSD
  price_account .. FCLf9N8xcN9HBA9Cw68FfEZoSBr4bYYJtyRxosNzswMH
    price_type ... price
    exponent ..... -9
    status ....... trading
    corp_act ..... nocorpact
    price ........ 55233535000000
    conf ......... 81650000000
    valid_slot ... 55519684
    publish_slot . 55519685
...

```

The details of each `product` account and their corresponding `price` accounts are dumped to stdout.

Each `product` may have a slightly different set of reference attributes, depending on their type, but all have "symbol", "asset_type", "quote_currency" and "tenor". US equity products, for example, include various additional reference symbology that is useful for mapping pyth products to other industry-standard identifiers.

Each `price` contains a "price" and "conf" value. "conf" represents a confidence interval on price and broadly corresponds to bid-offer spread.  All "price"s are stored as 64bit integers with a fixed, implied number of decimal places defined by the "exponent" field. The AAPL price of 12276250 above, therefore, respresents a value of 122.76250 because the "exponent" is set at -5 or 5 decimal places.

Each `price` has a "status" which is an enumeration of the following values: "trading", "halted", "auction" or "unknown".  Only "trading" prices are valid.  Equity products also contain a "corp_act" status to notify users of any ongoing corprate action event that may affect a product's price or symbology.

The "valid_slot" and "publish_slot" fields correspond to which solana slots the aggregate price was published in.  More on this in the next section.


## Aggregate Price Procedure

The pyth price represents an aggregate derived from multiple contributing market "quoters".  It is a two-stage process to derive the aggregate price: first, individual quoters submit their prices along with what they believe to be the most recently confirmed solana slot value.  The second stage gathers the latest prices from each quoter, discards those that are too old or not in a valid trading state and derives an aggregate price, currently through a simple median.

The pyth program accumulates prices with respect to whatever the current slot is inside the solana node. This is called the "valid_slot" above. As soon as the slot ticks forward by one, the pyth program computes the aggregate price and publishes it with respect to the new "publish_slot" and starts repeating the process with a new "valid_slot".

The aggregate "status" of a price is subject to whether there are any valid contributors (i.e. "unknown" status) or whether any contributors are in a "halted" or "auction" state.
