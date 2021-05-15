# pyth-client

## A library for parsing on-chain Pyth oracle data

[Pyth](https://pyth.network/) is building a way to deliver a decentralized, cross-chain market of verifiable data from high-quality nodes to any smart contract, anywhere.

This library consumes on-chain Pyth `accountInfo.data` from [@solana/web3.js](https://www.npmjs.com/package/@solana/web3.js) and returns JavaScript-friendly objects.

See our [examples repo](https://github.com/pyth-network/pyth-examples) for real-world usage examples.

## Example Usage

```
import { Connection, PublicKey } from '@solana/web3.js'
import { parseMappingData, parsePriceData, parseProductData } from '@pythnetwork/pyth-client'
const connection = new Connection(SOLANA_CLUSTER_URL)
  const publicKey = new PublicKey(ORACLE_MAPPING_PUBLIC_KEY)
  connection.getAccountInfo(publicKey).then((accountInfo) => {
    const { productAccountKeys } = parseMappingData(accountInfo.data)
    connection.getAccountInfo(productAccountKeys[productAccountKeys.length - 1]).then((accountInfo) => {
      const { product, priceAccountKey } = parseProductData(accountInfo.data)
      connection.getAccountInfo(priceAccountKey).then((accountInfo) => {
        const { price, confidence } = parsePriceData(accountInfo.data)
        console.log(`${product.symbol}: $${price} \xB1$${confidence}`)
        // SRM/USD: $8.68725 ±$0.0131
      })
    })
  })
```

To get streaming price updates, you may want to use `connection.onAccountChange`
