from typing import Dict
from construct import Bytes, Int32sl, Int32ul, Struct
from solana.publickey import PublicKey
from solana.transaction import AccountMeta, TransactionInstruction, Transaction
from solana.keypair import Keypair
from solana.rpc.api import Client
import base64
import os
import asyncio

import json
import time
from subprocess import check_call, check_output


def test_publish(solana_test_validator, pyth_dir,
                 pyth_init_product, pyth_init_price, solana_keygen, solana_program_deploy):

    def get_price_acct():
        cmd = [
            'pyth', 'get_product',
            pyth_init_product['LTC'],  # TODO get_price method
            '-r', 'localhost',
            '-k', pyth_dir,
            '-c', 'finalized',
            '-j',
        ]
        output = check_output(cmd)
        output = output.decode('ascii')
        output = json.loads(output)
        return output['price_accounts'][0]
    
    before = get_price_acct()
    assert before['publisher_accounts'][0]['price'] == 0
    assert before['publisher_accounts'][0]['conf'] == 0
    assert before['publisher_accounts'][0]['status'] == 'unknown'

    cmd = [
        'pyth', 'upd_price_val',
        pyth_init_price['LTC'],
        '150', '7', 'trading',
        '-r', 'localhost',
        '-k', pyth_dir,
        '-c', 'finalized',
        '-x',
    ]
    check_call(cmd)

    time.sleep(20)

    cmd = [
        'pyth', 'upd_price',
        pyth_init_price['LTC'],
        '-r', 'localhost',
        '-k', pyth_dir,
        '-c', 'finalized',
        '-x',
    ]
    check_call(cmd)

    time.sleep(20)
    after = get_price_acct()
    assert after['publisher_accounts'][0]['price'] == 150
    assert after['publisher_accounts'][0]['conf'] == 7
    assert after['publisher_accounts'][0]['status'] == 'trading'
    
    cmd = [
        'pyth', 'upd_price_val',
        pyth_init_price['LTC'],
        '100', '1', 'trading',
        '-r', 'localhost',
        '-k', pyth_dir,
        '-c', 'finalized',
        '-x',
    ]
    check_call(cmd)

    time.sleep(20)

    cmd = [
        'pyth', 'upd_price',
        pyth_init_price['LTC'],
        '-r', 'localhost',
        '-k', pyth_dir,
        '-c', 'finalized',
        '-x',
    ]
    check_call(cmd)

    time.sleep(20)

    after = get_price_acct()
    assert after['publisher_accounts'][0]['price'] == 100
    assert after['publisher_accounts'][0]['conf'] == 1
    assert after['publisher_accounts'][0]['status'] == 'trading'
