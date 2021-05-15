import { Connection, PublicKey } from '@solana/web3.js'
import { parseMappingData, Magic, Version } from '../index'

test('Mapping', (done) => {
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
        console.log(mapping)
        expect(mapping.magic).toBe(Magic)
        expect(mapping.version).toBe(Version)
        done()
      } else {
        done('No mapping accountInfo')
      }
    })
    .catch((error) => {
      done(error)
    })
})
