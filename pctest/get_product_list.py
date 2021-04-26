#!/usr/bin/python3

# pip3 install websockets
import asyncio
import websockets
import json
import datetime
import sys

async def get_symbol_list( uri ):

  # connect to pythd
  ws = await websockets.connect(uri)

  # submit get_symbol_list request
  req = { 'jsonrpc': '2.0', 'method':  'get_product_list', 'id': None }
  await ws.send( json.dumps( req ) )

  # wait for reply
  msg = json.loads( await ws.recv() )

  # print result
  print( json.dumps( msg, indent=True ) )

if __name__ == '__main__':
  uri='ws://localhost:8910'
  eloop = asyncio.get_event_loop()
  try:
    eloop.run_until_complete( get_symbol_list( uri ) )
  except ConnectionRefusedError:
    print( f'connection refused uri={uri}' )
    sys.exit(1)
