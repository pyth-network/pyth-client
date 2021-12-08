import json
import time
from subprocess import check_call, check_output


def test_publish(solana_test_validator, pyth_dir,
                 pyth_init_product, pyth_init_price):

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
        '150', '10', 'trading',
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
    assert after['publisher_accounts'][0]['conf'] == 10
    assert after['publisher_accounts'][0]['status'] == 'trading'
