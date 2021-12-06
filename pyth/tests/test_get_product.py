import asyncio
import json
import websockets

def test_get_product_list(solana_test_validator, pythd, pyth_init_price):

    data = {
      'jsonrpc': '2.0',
      'id': 1,
      'method': 'get_product_list',
    }
    data = json.dumps(data)
    async def f():
        async with websockets.connect('ws://localhost:8910/') as ws:
            await ws.send(data)
            result = await ws.recv()
            return result
    result = asyncio.run(f())
    result = json.loads(result)
    result = result['result']
    result = result[0]
    del result['account']  # TODO
    expected_result = {
        'attr_dict': {
            'symbol': 'LTC/USD',
            'asset_type': 'Crypto',
            'country': 'US',
            'quote_currency': 'USD',
            'tenor': 'Spot',
            'jlqd_symbol': 'LTCUSD',
        },
        'price': [
            {
                'account': pyth_init_price,
                'price_exponent': -5,
                'price_type': 'price',
            },
        ],
    }
    assert result == expected_result
