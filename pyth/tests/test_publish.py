

from typing import Dict

from construct import Bytes, Int32sl, Int32ul, Struct
from solana.publickey import PublicKey
from solana.transaction import AccountMeta, TransactionInstruction, Transaction
from solana.keypair import Keypair
from solana.rpc.api import Client

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
    
    #constants from oracle.h
    PROGRAM_VERSION = 2 #TODO: update this
    COMMAND_UPD_ACCOUNT = 14

    layout = Struct("version" / Int32ul, "command" / Int32sl)
    data = layout.build(dict(version=PROGRAM_VERSION, command=COMMAND_UPD_ACCOUNT))
    funding_key = PublicKey(solana_keygen[0])
    price_key = PublicKey(before["account"])
    system_key = PublicKey("11111111111111111111111111111111")
    print("program id is", solana_program_deploy)
    resize_instruction = TransactionInstruction(
        data=data,
        keys=[
            AccountMeta(pubkey=funding_key, is_signer=True, is_writable=True),
            AccountMeta(pubkey=price_key, is_signer=False, is_writable=True),
            AccountMeta(pubkey=system_key, is_signer=False, is_writable=False),
        ],
        program_id = PublicKey(solana_program_deploy),
    )
    print("key pair", type(solana_keygen[1]), solana_keygen[1])
    txn = Transaction().add(resize_instruction)
    solana_client = Client("http://localhost:8899")
    key_file = open(solana_keygen[1])
    key_data = json.load(key_file)
    key_file.close()

    sender = Keypair.from_secret_key(key_data)
    txn.sign(sender)
    print(solana_client.send_transaction(txn, sender))

    time.sleep(40)

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


