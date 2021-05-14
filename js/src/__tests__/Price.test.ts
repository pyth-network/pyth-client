import { Connection, PublicKey } from '@solana/web3.js'
import { Magic, parseMappingData, parsePriceData, parseProductData, Version } from '../index'

test('Price', (done) => {
  jest.setTimeout(60000)
  const url = 'https://devnet.solana.com'
  const oraclePublicKey = 'ArppEFcsybCLE8CRtQJLQ9tLv2peGmQoKWFuiUWm4KBP'
  const connection = new Connection(url)
  const publicKey = new PublicKey(oraclePublicKey)
  connection
    .getAccountInfo(publicKey)
    .then((accountInfo) => {
      if (accountInfo && accountInfo.data) {
        const mapping = parseMappingData(accountInfo.data)
        connection
          .getAccountInfo(mapping.productAccountKeys[mapping.productAccountKeys.length - 1])
          .then((accountInfo) => {
            if (accountInfo && accountInfo.data) {
              const product = parseProductData(accountInfo.data)
              connection.getAccountInfo(product.priceAccountKey).then((accountInfo) => {
                if (accountInfo && accountInfo.data) {
                  const price = parsePriceData(accountInfo.data)
                  console.log(price)
                  expect(price.magic).toBe(Magic)
                  expect(price.version).toBe(Version)
                  done()
                } else {
                  done('No price accountInfo')
                }
              })
            } else {
              done('No product accountInfo')
            }
          })
      } else {
        done('No mapping accountInfo')
      }
    })
    .catch((error) => {
      done(error)
    })
})
