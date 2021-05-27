#!/usr/bin/python3

import sys
import json
import subprocess

if __name__ == '__main__':
  if len(sys.argv)<3:
    print( 'usage: add_publisher.py <config_file> <pub_key>',
           file=sys.stderr )
    sys.exit(1)
  conf_file = sys.argv[1]
  pub_key = sys.argv[2]

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
    # add publisher to each price account
    for pxa in prod['price_accounts']:
      px_acct = pxa['account']
      cmds = [ './pyth', 'add_publisher', pub_key, px_acct,
          '-k', key_store, '-r', rpc_host, '-n' ]
      print( cmds, file=sys.stderr )
      res = subprocess.run( cmds, capture_output=True, text=True )
      if res.returncode != 0:
        print( res.stderr, file=sys.stderr );
        sys.exit(1)
