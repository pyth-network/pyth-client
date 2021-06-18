#!/usr/bin/python3

# pip3 install websockets
import asyncio
import websockets
import json
import datetime
import sys

class test_publish:
  idnum = 1

  def __init__( self, sym, price, spread ):
    self.symbol = sym
    self.pidnum = test_publish.idnum
    test_publish.idnum += 1
    self.sidnum = test_publish.idnum
    test_publish.idnum += 1
    self.psubid = -1
    self.ssubid = -1
    self.price  = price
    self.spread = spread

  def gen_subscribe_price(self):
    req = {
        'jsonrpc': '2.0',
        'method' : 'subscribe_price',
        'params' : {
          'account': self.account,
          'price_type' : 'price'
          },
        'id': self.sidnum
        }
    return json.dumps( req )

  def gen_subscribe_price_sched(self):
    req = {
        'jsonrpc': '2.0',
        'method' : 'subscribe_price_sched',
        'params' : {
          'account': self.account,
          'price_type' : 'price'
          },
        'id': self.pidnum
        }
    return json.dumps( req )

  def gen_update_price(self):
    req = {
        'jsonrpc': '2.0',
        'method':  'update_price',
        'params':{
          'account': self.account,
          'price_type': 'price',
          'status': 'trading',
          'price': self.price,
          'conf': self.spread
          },
        'id': None
        }
    self.price += self.spread
    return json.dumps( req )

  def parse_reply( self, msg, allsub ):
    # parse subscription replies
    subid = msg['result']['subscription']
    allsub[subid] = self
    if msg['id'] == self.pidnum:
      self.psubid = subid;
    else:
      self.ssubid = subid

  async def parse_notify( self, ws, msg ):
    # parse subscription notification messages
    subid = msg['params']['subscription']
    ts = datetime.datetime.utcnow().isoformat()
    if subid == self.ssubid:
      # aggregate price update
      res = msg['params']['result']
      price  = res['price']
      spread = res['conf']
      status = res['status']
      twap   = res['twap']
      twac   = res['twac']
      print( f'{ts} received aggregate price update symbol=' + self.symbol +
          f',price={price}, spread={spread}, twap={twap}, twac={twac}, '
          f'status={status}' )
    else:
      # request to submit price
      print( f'{ts} submit price to block-chain symbol=' + self.symbol +
          f',price={self.price}, spread={self.spread}, subscription={subid}')
      await ws.send( self.gen_update_price() )

  async def subscribe( self, acct, ws, allids ):
    # submmit initial subscriptions
    self.account = acct
    allids[self.pidnum] = self
    allids[self.sidnum] = self
    await ws.send( self.gen_subscribe_price() )
    await ws.send( self.gen_subscribe_price_sched() )


# wbsocket event loop
async def poll( uri ):
  # connect to pythd
  ws = await websockets.connect(uri)

  # submit subscriptions to pythd
  allids = {}
  allsub = {}
  allsym = {}
  sym1 = test_publish( 'SYMBOL1/USD', 10000, 100 )
  sym2 = test_publish( 'SYMBOL2/USD', 2000000, 20000 )
  allsym[sym1.symbol] = sym1
  allsym[sym2.symbol] = sym2

  # lookup accounts by symbol and subscribe
  req = { 'jsonrpc': '2.0', 'method':  'get_product_list', 'id': None }
  await ws.send( json.dumps( req ) )
  msg = json.loads( await ws.recv() )
  for prod in msg['result']:
    sym = prod['attr_dict']['symbol']
    for px in prod['price']:
      if sym in allsym and px['price_type'] == 'price':
        await allsym[sym].subscribe( px['account'], ws, allids );

  # poll for updates from pythd
  while True:
    msg = json.loads( await ws.recv() )
#    print(msg)
    if 'error' in msg:
      ts = datetime.datetime.utcnow().isoformat()
      code = msg['error']['code']
      emsg = msg['error']['message']
      print( f'{ts} error code: {code} msg: {emsg}' )
      sys.exit(1)
    elif 'result' in msg:
      msgid = msg['id']
      if msgid in allids:
        allids[msgid].parse_reply( msg, allsub )
    else:
      subid = msg['params']['subscription']
      if subid in allsub:
        await allsub[subid].parse_notify( ws, msg )

# connect to pythd, subscribe to and start publishing on two symbols
if __name__ == '__main__':
  uri='ws://localhost:8910'
  eloop = asyncio.get_event_loop()
  try:
    eloop.run_until_complete( poll( uri ) )
  except ConnectionRefusedError:
    print( f'connection refused uri={uri}' )
    sys.exit(1)
