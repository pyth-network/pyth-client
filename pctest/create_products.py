#!/usr/bin/python3

import sys
import json
import subprocess

def run_cmd( cmds ) :
  print( cmds, file=sys.stderr )
  res = subprocess.run( cmds, capture_output=True, text=True )
  if res.returncode != 0:
    print( res.stderr, file=sys.stderr );
    sys.exit(1)
  return res

if __name__ == '__main__':
  if len(sys.argv)<2:
    print( 'usage: create_products.py <config_file>',
           file=sys.stderr )
    sys.exit(1)
  conf_file = sys.argv[1]

  # read config file
  cf = open( conf_file )
  cd = json.loads( cf.read() )
  cf.close()
  key_store = cd['key_store']
  rpc_host  = cd['rpc_host']
  prod_file = cd['product_file']

  # read products file
  pf = open( prod_file )
  pd = json.loads( pf.read() )
  pf.close()

  # process each product
  for prod in pd:
    # create product account
    cmds = [ './pyth_admin', 'add_product', '-k', key_store, '-r', rpc_host,
        '-c', 'finalized' ]
    res = run_cmd( cmds )
    prod_acct = res.stdout.strip()
    prod['account'] = prod_acct

    # create price accounts for this product
    for pxa in prod['price_accounts']:
      px_type = pxa['price_type']
      px_expo = pxa['price_exponent']
      cmds = [ './pyth_admin', 'add_price', prod['account'], px_type,
          '-e', str(px_expo), '-k', key_store, '-r', rpc_host,
          '-c', 'finalized', '-n' ]
      res = run_cmd( cmds )
      pxa['account'] = res.stdout.strip()

      # add corresponding publishers
      for pub in pxa['publisher_accounts']:
        cmds = [ './pyth_admin', 'add_publisher', pub['account'], pxa['account'],
          '-k', key_store, '-r', rpc_host, '-c', 'finalized', '-n' ]
        run_cmd( cmds )

  # write back updated products file
  pf = open( prod_file, "w+" )
  pf.write( json.dumps( pd, indent=True ) )
  pf.close()

  # finally generate product reference data
  cmds = [ './pyth_admin', 'upd_product', prod_file,
           '-k', key_store, '-r', rpc_host ]
  res = run_cmd( cmds )
