#!/usr/bin/python3

# pip3 install websockets
import asyncio
import websockets
import json
import sys
import random

def check_expr( expr, msg ):
  if not expr:
    print( f'expr validation failed: {msg}' )
    sys.exit(1)

def check_error( ecode, msg, idval ):
  if msg['jsonrpc'] != '2.0':
    print( f'missing jsonrpc' )
    sys.exit(1)
  if msg['id'] != idval:
    print( f'missing expected idval' )
    sys.exit(1)
  emsg = msg['error']
  rcode = emsg['code']
  if rcode != ecode:
    print( f'missing code: expected={code} received={rcode}' )

def check_error_obj(ecode, msg, idval=None ):
  print(msg)
  check_error( ecode, json.loads( msg ), idval )

def check_error_arr(ecode, msg, idval=None ):
  print(msg)
  jmsg = json.loads( msg )
  for imsg in jmsg:
    check_error( ecode, imsg, idval )

# basic bad request error checking
async def test_1(uri):
  ws = await websockets.connect(uri)

  # ping/pong test
  pw = await ws.ping()
  await pw

  # bad json
  await ws.send( 'foo' )
  check_error_obj( -32700, await ws.recv() )
  # invalid json array
  await ws.send( '["hello"]' )
  check_error_arr( -32600, await ws.recv() )
  # empty array
  await ws.send( '[]' )
  check_error_obj( -32600, await ws.recv() )
  # bad id/method
  await ws.send( '{"id":{},"method":"get_symbol_list"}' )
  check_error_obj( -32600, await ws.recv() )
  await ws.send( '{"id":null,"method":[]}' )
  check_error_obj( -32600, await ws.recv() )
  await ws.send( '{"id":null,"method":"non-existant"}' )
  check_error_obj( -32601, await ws.recv() )
  # multibad request
  await ws.send( '[{"id":null,"method":"non-existant"},{"id":null,"method":4}]')
  check_error_arr( -32601, await ws.recv() )

  # get symbol list
  await ws.send( '{"id":42,"method":"get_symbol_list"}' )
  slist_s = await ws.recv()
  print( slist_s )
  slist = json.loads( slist_s )
  check_expr( slist['id'] == 42, 'id mismatch' )
  sres = slist['result']
  check_expr( len(sres) == 3, 'expecting 3 symbols')
  check_expr( sres[0]['symbol'] == 'US.EQ.SYMBOL1', 'symbol1 check' )
  check_expr( sres[0]['price_exponent'] == -4, 'symbol1 expo check' )
  check_expr( sres[1]['symbol'] == 'US.EQ.SYMBOL2', 'symbol2 check' )
  check_expr( sres[1]['price_exponent'] == -6, 'symbol2 expo check' )
  check_expr( sres[2]['symbol'] == 'US.EQ.SYMBOL3', 'symbol3 check' )
  check_expr( sres[2]['price_exponent'] == -2, 'symbol3 expo check' )

  # subscribe to symbol
  await ws.send( '{"id":42,"method":"subscribe_price", "params", {}}' )
  check_error_obj( -32602, await ws.recv(), idval=42 )
  await ws.send( '{"id":42,"method":"subscribe_price", "params": {}}' )
  check_error_obj( -32602, await ws.recv(), idval=42 )
  await ws.send( '{"id":42,"method":"subscribe_price", "params": '
    '{"symbol":"US.EQ.SYMBOL1", "price_type":"price"}}' )
  # get subscription result
  pres_s = await ws.recv()
  print( pres_s )
  pres = json.loads( pres_s )
  check_expr( pres['result']['subscription'] == 0, 'subscription result' )
  # immediate get first price update
  pres_s = await ws.recv()
  print( pres_s )
  pres = json.loads( pres_s )
  check_expr( pres['method'] == 'notify_price', 'first notify' )
  prms = pres['params']
  pdet = prms['result']
  check_expr( 'price' in pdet, 'missing price field' )
  check_expr( 'conf' in pdet, 'missing conf field' )
  check_expr( 'status' in pdet, 'missing status field' )
  check_expr( pdet['status'] in ['unknown','trading','halted'],
              'bad status field' )
  check_expr( prms['subscription'] == 0, 'subscription id' )

  # second subscription
  await ws.send( '{"id":10,"method":"subscribe_price", "params": '
      '{"symbol":"US.EQ.SYMBOL2", "price_type": "price"}}' )
  pres_s = await ws.recv()
  print( pres_s )
  pres = json.loads( pres_s )
  check_expr( pres['result']['subscription'] == 1, 'subscription result' )
  await ws.recv()

  # send bad price updates
  await ws.send( '{"id":null,"method":"update_price"}' )
  check_error_obj( -32602, await ws.recv() )
  await ws.send( '{"id":null,"method":"update_price", "params":[]}' )
  check_error_obj( -32602, await ws.recv() )
  await ws.send( '{"id":null,"method":"update_price", "params":["foo"]}' )
  check_error_obj( -32602, await ws.recv() )
  # unknown symbol update check
  await ws.send( '{"id":18,"method":"update_price", "params":'
      '{"symbol":"foo", "price_type": "price", "price" : 42, "conf": 1, '
      '"status":"trading"}}')
  check_error_obj( -32000, await ws.recv(), idval=18 )


def gen_sub(sym,idnum):
  return {
      'jsonrpc':'2.0',
      'method': 'subscribe_price',
      'params': { 'symbol': sym, 'price_type': 'price' },
      'id':idnum }

def check_notify(res,fld):
  check_expr( fld in res['result'], fild + ' missing in result' )

# simple price submission/subscription across multiple pythd servers
async def test_2(uri1,uri2):
  ws1 = await websockets.connect(uri1)
  ws2 = await websockets.connect(uri2)
  # subscribe to symbol1, symbol2 and symbol3 on both servers
  # note: ws2 does not have publishing rights to symbol3
  print( 'check invalid publisher for symbol3' )
  await ws2.send( '{"id":18,"method":"update_price", "params":'
    '{"symbol":"US.EQ.SYMBOL3", "price_type": "price", "price": 1, '
    '"conf":2, "status": "trading"}}' )
  check_error_obj( -32001, await ws2.recv(), idval=18 )

  print('submitting subscriptions for symbols1, 2, 3')
  req = [ gen_sub( 'US.EQ.SYMBOL1', 1 ),
          gen_sub( 'US.EQ.SYMBOL2', 2 ),
          gen_sub( 'US.EQ.SYMBOL3', 3 ) ]
  await ws1.send( json.dumps(req) )
  await ws2.send( json.dumps(req) )
  resp1 = json.loads( await ws1.recv() )
  resp2 = json.loads( await ws2.recv() )
  print( resp1 )
  print( resp2 )
  check_expr( resp1 == resp2, 'subsription responses the same' )
  check_expr( len(resp1)==3, '3 subscription responses in one reply' )
  check_expr( resp1[0]['result']['subscription'] == 0, 'first sub' )
  check_expr( resp1[1]['result']['subscription'] == 1, 'second sub' )
  check_expr( resp1[2]['result']['subscription'] == 2, 'third sub' )

  # get notifications
  for i in range(3):
    resp1 = json.loads( await ws1.recv() )
    resp2 = json.loads( await ws2.recv() )
    print( resp1 )
    print( resp2 )
    check_expr( resp1 == resp2, 'subsription responses the same' )
    check_expr( resp1['method'] == 'notify_price', 'expecting notify' )
    prms = resp1['params']
    check_expr( prms['subscription'] == i, 'expecting notify sub' )
    check_expr( 'price' in prms['result'], 'missing price' )
    check_expr( 'conf' in prms['result'], 'missing conf' )
    check_expr( 'status' in prms['result'], 'missing status' )

  last_px = 0
  last_conf = 0
  for i in range(3):
    # submit price for symbol 2
    print( 'update price for symbol 2 : try ' + str(i) )
    px = random.randint( 1000, 20000 )
    conf = random.randint( 10, 200 )
    req = { 'jsonrpc': '2.0', 'method': 'update_price', 'params':
        { 'symbol':'US.EQ.SYMBOL2', 'price_type': 'price',
          'price':px, 'conf': conf, 'status': 'trading' }, 'id': i+10 }
    print( 'sending ' + json.dumps(req) )
    await ws1.send( json.dumps(req) )
    resp1 = json.loads( await ws1.recv() )
    print( resp1 )
    check_expr( resp1['result'] == 0, 'clean price update' )
    check_expr( resp1['id'] == i+10, 'correct response id' )
    resp1 = json.loads( await ws1.recv() )
    resp2 = json.loads( await ws2.recv() )
    print( resp1 )
    print( resp2 )
    check_expr( resp1 == resp2, 'subsription responses the same' )
    check_expr( resp1['method'] == 'notify_price', 'expecting notify' )
    prms = resp1['params']
    check_expr( prms['subscription'] == 1, 'expecting notify sub symbol2' )
    if i>0:
      check_expr( prms['result']['price'] == last_px, 'incorrect price' )
      check_expr( prms['result']['conf'] == last_conf, 'incorrect conf' )
      check_expr( prms['result']['status'] == 'trading', 'incorrect status' )
    last_px = px
    last_conf = conf

async def test():
  uri1='ws://localhost:8910'
  uri2='ws://localhost:8911'
  await test_1( uri1 )
  await test_2( uri1, uri2 )

if __name__ == '__main__':
  eloop = asyncio.get_event_loop()
  try:
    eloop.run_until_complete( test() )
  except ConnectionRefusedError:
    print( f'connection refused uri={uri}' )
    sys.exit(1)
