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
    print( 'usage: init_price.py <config_file>',
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
    # update each  price accounts for this product
    for pxa in prod['price_accounts']:
      px_expo = pxa['price_exponent']
      cmds = [ './pyth', 'init_price', pxa['account'],
          '-e', str(px_expo), '-k', key_store, '-r', rpc_host, '-n' ]
      run_cmd( cmds )
