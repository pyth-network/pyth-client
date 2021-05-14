import { Connection, PublicKey } from '@solana/web3.js'
import { parseMappingData, parsePriceData, parseProductData } from '../index'

const SOLANA_CLUSTER_URL = 'https://devnet.solana.com'
const ORACLE_MAPPING_PUBLIC_KEY = 'ArppEFcsybCLE8CRtQJLQ9tLv2peGmQoKWFuiUWm4KBP'

test('Mapping', (done) => {
  jest.setTimeout(60000)
  const connection = new Connection(SOLANA_CLUSTER_URL)
  const publicKey = new PublicKey(ORACLE_MAPPING_PUBLIC_KEY)
  connection.getAccountInfo(publicKey).then((accountInfo) => {
    if (!accountInfo) {
      done('No mapping accountInfo')
      return
    }
    const { productAccountKeys } = parseMappingData(accountInfo.data)
    connection.getAccountInfo(productAccountKeys[productAccountKeys.length - 1]).then((accountInfo) => {
      if (!accountInfo) {
        done('No product accountInfo')
        return
      }
      const { product, priceAccountKey } = parseProductData(accountInfo.data)
      connection.getAccountInfo(priceAccountKey).then((accountInfo) => {
        if (!accountInfo) {
          done('No price accountInfo')
          return
        }
        const { price, confidence } = parsePriceData(accountInfo.data)
        console.log(`${product.symbol}: $${price} \xB1$${confidence}`)
        // SRM/USD: $8.68725 ±$0.0131
        done()
      })
    })
  })
})
