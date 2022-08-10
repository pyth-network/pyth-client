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
        key_file = open(path_to_file)
        key_data = json.load(key_file)
        key_file.close()

        return Keypair.from_secret_key(key_data)
    
    def get_path_to_pythdir_pair(account_addr):
        return os.path.join(pyth_dir, "account_" + str(account_addr) + ".json")


    
    def resize_account(price_acc_address):
        """
        given a string with the pubkey of a price accountm it calls the resize instruction of the Oracle on it
        """
        #constants from oracle.h
        PROGRAM_VERSION = 2 #TODO: update this
        COMMAND_UPD_ACCOUNT = 14
        SYSTEM_PROGRAM = "11111111111111111111111111111111"

        #update version of price accounts to make sure they resize
        layout = Struct("version" / Int32ul, "command" / Int32sl)
        data = layout.build(dict(version=PROGRAM_VERSION, command=COMMAND_UPD_ACCOUNT))
        funding_key = PublicKey(solana_keygen[0])
        price_key = PublicKey(price_acc_address)
        system_key = PublicKey(SYSTEM_PROGRAM)
        print("program id is", solana_program_deploy)
        resize_instruction = TransactionInstruction(
            data=data,
            keys=[
                AccountMeta(pubkey=funding_key, is_signer=True, is_writable=True),
                AccountMeta(pubkey=price_key, is_signer=True, is_writable=True),
                AccountMeta(pubkey=system_key, is_signer=False, is_writable=False),
            ],
            program_id = PublicKey(solana_program_deploy),
        )
        txn = Transaction().add(resize_instruction)
        solana_client = Client("http://localhost:8899")

        sender = get_key_pair(solana_keygen[1])
        path_to_price =  get_path_to_pythdir_pair(price_key)
        price_key_pair = get_key_pair(path_to_price)
        solana_client.send_transaction(txn, sender, price_key_pair)

    def get_account_size(acc_address):
        """
        given a string with the pubkey of an account, resize it
        """
        PublicKey(acc_address)
        solana_client = Client("http://localhost:8899")
        data = solana_client.get_account_info(PublicKey(acc_address), encoding = 'base64')['result']['value']['data'][0]
        data = base64.b64decode(data)
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

    resize_account(pyth_init_price['LTC'])
    time.sleep(20)
    #defined in oracle.h
    new_account_size = 6176
    assert get_account_size(pyth_init_price['LTC']) == new_account_size


    
    #try adding a new price to the resized accounts
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
