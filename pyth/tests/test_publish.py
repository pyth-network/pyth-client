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
    
    def get_key_pair(path_to_file):
        with  open(path_to_file) as key_file:
            key_data = json.load(key_file)
        return Keypair.from_secret_key(key_data)
    
    def get_path_to_pythdir_pair(account_addr):
        return os.path.join(pyth_dir, "account_" + str(account_addr) + ".json")


    def get_account_size(acc_address):
        """
        given a string with the pubkey of an account, return its size
        """
        PublicKey(acc_address)
        solana_client = Client("http://localhost:8899")
        data = solana_client.get_account_info(PublicKey(acc_address), encoding = 'base64').value.data
        return len(data)
        

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

    time.sleep(20)
    #defined in oracle.h
    new_account_size = 6176
    assert get_account_size(pyth_init_price['LTC']) == new_account_size


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
