#!/usr/bin/python3

import sys
import json
import subprocess

if __name__ == '__main__':
  if len(sys.argv)<2:
    print( 'usage: get_products.py <config_file>',
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
  pd= []

  # get product list
  cmd = [ './pyth', 'get_product_list', '-k', key_store,
      '-r', rpc_host, '-j' ]
  print( cmd, file=sys.stderr )
  res = subprocess.run( cmd, capture_output=True, text=True )
  if res.returncode != 0:
    print( res.stderr, file=sys.stderr );
    sys.exit(1)
  prod_list = json.loads( res.stdout.strip() )
  for prod in prod_list:
    # get product
    cmd = [ './pyth', 'get_product', prod['account'], '-k', key_store,
        '-r', rpc_host, '-j' ]
    print( cmd, file=sys.stderr )
    res = subprocess.run( cmd, capture_output=True, text=True )
    if res.returncode != 0:
      print( res.stderr, file=sys.stderr );
      sys.exit(1)
    prod = json.loads( res.stdout.strip() )
    pres = {}
    pres['account']   = prod['account']
    pres['attr_dict'] = prod['attr_dict']
    pres['price_accounts'] = []
    pd.append( pres );
    for px in prod['price_accounts']:
      xres = {}
      xres['price_exponent'] = px['price_exponent']
      xres['price_type'] = px['price_type']
      xres['account'] = px['account']
      xres['publisher_accounts'] = []
      pres['price_accounts'].append( xres )
      for pub in px['publisher_accounts']:
        ures = {}
        ures['account'] = pub['account']
        xres['publisher_accounts'].append( ures )

  # write products file
  pf = open( prod_file, "w+" )
  pf.write( json.dumps( pd, indent=True ) )
  pf.close()
