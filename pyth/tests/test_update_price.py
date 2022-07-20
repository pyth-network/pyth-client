import json
from subprocess import check_output
import pytest
import websockets
import time
import itertools
import random

from pyth.tests.conftest import PRODUCTS

@pytest.mark.asyncio
async def test_batch_update_price(solana_test_validator, solana_logs, pythd, pyth_dir, pyth_init_product, pyth_init_price):

    messageIds = itertools.count()

    # Use a single websocket connection for the entire test, as batching is done per-user
    async with websockets.connect('ws://localhost:8910/') as ws:

        async def update_price(account, price, conf, status):
            msg = jrpc_req(
                method='update_price',
                params={
                    'account': account,
                    'price': price,
                    'conf': conf,
                    'status': status,
                })

            await send(msg)
            resp = await recv()
            assert resp['result'] == 0

        async def get_product(account):
            output = check_output([
                'pyth', 'get_product',
                account,
                '-r', 'localhost',
                '-k', pyth_dir,
                '-c', 'finalized',
                '-j',
            ]).decode('ascii')
            result = json.loads(output)

            return result

        def jrpc_req(method=None, params=None):
            return {
                'jsonrpc': '2.0',
                'method': method,
                'params': params,
                'id': next(messageIds)
            }

        async def send(msg):
            print("--- sending message ---")
            print(msg)
            await ws.send(json.dumps(msg))

        async def recv():
            data = await ws.recv()
            msg = json.loads(data)
            print("----- received message -----")
            print(msg)
            return msg

        def get_publisher_acc(product_acc):
            assert len(product_acc['price_accounts']) == 1
            price_acc = product_acc['price_accounts'][0]

            assert len(price_acc['publisher_accounts']) == 1
            return price_acc['publisher_accounts'][0]

        # Check that the prices are 0 initially
        for product in PRODUCTS.keys():
            product_acc = await get_product(pyth_init_product[product])
            publisher_acc = get_publisher_acc(product_acc)

            assert publisher_acc['price'] == 0
            assert publisher_acc['conf'] == 0
            assert publisher_acc['status'] == 'unknown'

        #time.sleep(600)

        # Generate new values for this test
        new_values = {
            product: {
                'price':random.randint(1, 150),
                'conf': random.randint(1, 20),
                'status': 'trading',
            } for product in PRODUCTS.keys()
        }

        # Update the values of the products
        for product in PRODUCTS.keys():
            await update_price(
                pyth_init_price[product],
                new_values[product]['price'],
                new_values[product]['conf'],
                new_values[product]['status'])

        time.sleep(80)

        # Crank the products
        for product in PRODUCTS.keys():
            await update_price(
                pyth_init_price[product],
                1,
                1,
                'trading')

        time.sleep(80)

        # Check that the price has been updated
        for product in PRODUCTS.keys():
            product_acc = await get_product(pyth_init_product[product])
            publisher_acc = get_publisher_acc(product_acc)

            assert publisher_acc['price'] == new_values[product]['price']
            assert publisher_acc['conf'] == new_values[product]['conf']

            conf_threshold = new_values[product]['price'] / 20
            expected_status = 'unknown' if new_values[product]['conf'] > conf_threshold else 'trading'
            assert publisher_acc['status'] == expected_status
