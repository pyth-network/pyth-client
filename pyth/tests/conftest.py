import argparse
import glob
import inspect
import json
import os
import subprocess
import time
from os import fdopen
from shutil import rmtree
from subprocess import DEVNULL, Popen, check_call, check_output
from tempfile import mkdtemp, mkstemp

import pytest

__this_file = os.path.abspath(__file__)
__this_dir = os.path.dirname(__this_file)

PRODUCTS = {
    'BTC': {
        'symbol': 'BTC/USD',
        'asset_type': 'Crypto',
        'country': 'US',
        'quote_currency': 'USD',
        'tenor': 'Spot',
    },
    'ETH': {
        'symbol': 'ETH/USD',
        'asset_type': 'Crypto',
        'country': 'US',
        'quote_currency': 'USD',
        'tenor': 'Spot',
    },
    'LTC': {
        'symbol': 'LTC/USD',
        'asset_type': 'Crypto',
        'country': 'US',
        'quote_currency': 'USD',
        'tenor': 'Spot',
    },
}


@pytest.fixture(scope='session')
def solana_test_validator():

    ledger_dir = mkdtemp(prefix='stv_')
    cmd = [
        'solana-test-validator',
        '--rpc-port', '8899',
        '--ledger', ledger_dir,
    ]
    kwargs = {
        'stdin': DEVNULL,
        'stdout': DEVNULL,
        'stderr': DEVNULL,
    }
    with Popen(cmd, **kwargs) as p:
        time.sleep(3)
        yield
        p.terminate()
    rmtree(ledger_dir)


@pytest.fixture(scope='session')
def solana_keygen():

    cfg_dir = mkdtemp(prefix='cfg_')
    path = os.path.join(cfg_dir, 'id.json')
    cmd = ['solana-keygen', 'new', '--no-passphrase', '--outfile', path]
    output = check_output(cmd)
    output = output.decode('ascii')
    output = output.splitlines()
    output = [line for line in output if 'pubkey' in line][0]
    output = output.split('pubkey: ')[1]
    yield (output, path)
    rmtree(cfg_dir)


@pytest.fixture(scope='session')
def solana_airdrop(solana_test_validator, solana_keygen):

    cmd = [
        'solana', 'airdrop', '100', solana_keygen[0],
        '--commitment', 'finalized',
        '--url', 'localhost',
        '--keypair', solana_keygen[1],
    ]
    check_call(cmd)


@pytest.fixture(scope='session')
def solana_program_deploy(
    solana_test_validator, solana_keygen, solana_airdrop
):

    cmd = [
        'solana', 'program', 'deploy',
        os.path.abspath(
            os.path.join(__this_dir, '..', '..', 'target', 'deploy','pyth_oracle.so')
        ),
        '--commitment', 'finalized',
        '--url', 'localhost',
        '--keypair', solana_keygen[1],
    ]
    output = check_output(cmd)
    output = output.decode('ascii')
    output = output.splitlines()
    output = [line for line in output if 'Program Id' in line][0]
    output = output.split('Program Id: ')[1]
    return output


@pytest.fixture(scope='session')
def pyth_dir():

    path = mkdtemp(prefix='pythd_')
    yield path
    rmtree(path)


@pytest.fixture(scope='session')
def pyth_publish_key(solana_keygen, pyth_dir):

    path = os.path.join(pyth_dir, 'publish_key_pair.json')
    os.symlink(solana_keygen[1], path)


@pytest.fixture(scope='session')
def pyth_program_key(solana_program_deploy, pyth_dir):

    pyth_path = os.path.join(pyth_dir, 'program_key.json')
    with open(pyth_path, 'w') as f:
        f.write(solana_program_deploy)


@pytest.fixture(scope='session')
def pyth_init_mapping(
    solana_test_validator, pyth_dir, pyth_publish_key, pyth_program_key
):

    cmd = [
        'pyth_admin', 'init_mapping',
        '-r', 'localhost',
        '-k', pyth_dir,
        '-c', 'finalized',
    ]
    check_call(cmd)


@pytest.fixture(scope='session')
def pyth_add_product(solana_test_validator, pyth_dir, pyth_init_mapping):

    result = {}
    for product in PRODUCTS.keys():
        cmd = [
            'pyth_admin', 'add_product',
            '-r', 'localhost',
            '-k', pyth_dir,
            '-c', 'finalized',
        ]
        output = check_output(cmd)
        output = output.decode('ascii')
        output = output.splitlines()
        result[product] = output[0]
    return result


@pytest.fixture(scope='session')
def pyth_init_product(solana_test_validator, pyth_dir, pyth_add_product):

    products = []
    for product in pyth_add_product.keys():
        products.append({
            'account': pyth_add_product[product],
            'attr_dict': PRODUCTS[product],
        })
    fd, path = mkstemp(suffix='.json', prefix='products_')
    with fdopen(fd, 'w') as f:
        json.dump(products, f)
    cmd = [
        'pyth_admin', 'upd_product', path,
        '-r', 'localhost',
        '-k', pyth_dir,
        '-c', 'finalized',
    ]
    check_call(cmd)
    os.remove(path)
    return pyth_add_product


@pytest.fixture(scope='session')
def pyth_add_price(solana_test_validator, pyth_dir, pyth_init_product):

    result = {}
    for product, key in pyth_init_product.items():
        cmd = [
            'pyth_admin', 'add_price',
            key, 'price', '-e', '-5',
            '-r', 'localhost',
            '-k', pyth_dir,
            '-c', 'finalized',
            '-n',
        ]
        output = check_output(cmd)
        output = output.decode('ascii')
        output = output.splitlines()
        result[product] = output[0]
    return result


@pytest.fixture(scope='session')
def pyth_add_publisher(
    solana_test_validator, solana_keygen, pyth_dir, pyth_add_price
):

    for product, key in pyth_add_price.items():
        cmd = [
            'pyth_admin', 'add_publisher',
            solana_keygen[0], key,
            '-r', 'localhost',
            '-k', pyth_dir,
            '-c', 'finalized',
            '-n',
        ]
        check_call(cmd)
    return pyth_add_price


@pytest.fixture(scope='session')
def solana_logs(solana_test_validator, solana_keygen):
    with open("solana_logs.txt", 'w') as f:
        cmd = [
            'solana', 'logs',
            '--url', 'localhost',
            '--keypair', solana_keygen[1],
        ]
        p = subprocess.Popen(cmd, stdout=f)
        yield
        p.kill()


@pytest.fixture(scope='function')
def pyth_init_price(solana_test_validator, pyth_dir, pyth_add_publisher):

    for product, key in pyth_add_publisher.items():
        cmd = [
            'pyth_admin', 'init_price',
            key, '-e', '-5',
            '-r', 'localhost',
            '-k', pyth_dir,
            '-c', 'finalized',
            '-n',
        ]
        check_call(cmd)
    return pyth_add_publisher


@pytest.fixture(scope='session')
def pythd(solana_test_validator, pyth_dir):

    cmd = [
        'pythd',
        '-r', 'localhost',
        '-k', pyth_dir,
        '-x',
        '-m', 'finalized',
        '-d',
        '-l', 'pyth_logs.txt',
    ]
    kwargs = {
        'stdin': DEVNULL,
    }
        
    with Popen(cmd, **kwargs) as p:
        time.sleep(3)
        yield
        p.terminate()
