'use strict';
class Prices
{
  constructor() {
    this.req   = [];
    this.sub   = [];
    this.reuse = [];
    this.symbols = {};
    this.fact  = [ 0, 1e-1, 1e-2, 1e-3, 1e-4, 1e-5, 1e-6, 1e-7, 1e-8 ];
  }
  send( msg, callback ) {
    let id = 0;
    if ( this.reuse.length > 0 ) {
      id = this.reuse.pop();
    } else {
      id = this.req.length;
      this.req.push( msg );
    }
    msg.id = id;
    msg.jsonrpc = '2.0';
    ws.send( JSON.stringify( msg ) );
    msg.on_response = callback;
    this.req[id] = msg;
  }
  on_response( msg ) {
    this.reuse.push( msg.id );
    this.req[msg.id].on_response( msg );
  }
  on_notify( msg ) {
    this.sub[msg.params.subscription]( msg );
  }
  on_price( sym, msg ) {
    let tab = document.getElementById( "prices" );
    let row = tab.rows[sym.idx];
    let res = msg.params.result;
    let col = 1;
    let expo = -sym.price_exponent;
    let px   = res.price * this.fact[expo];
    let conf = res.conf * this.fact[expo];
    row.cells[col++].textContent = px.toFixed(expo);
    row.cells[col++].textContent = conf.toFixed(expo);
    row.cells[col++].textContent = res.status;
    row.cells[col++].textContent = res.valid_slot;
    row.cells[col++].textContent = res.pub_slot;
  }
  get_price( sym, msg ) {
    let sub_id = msg.result.subscription;
    while( sub_id < this.sub.length ) {
      this.sub.append( 0 )
    }
    this.sub[sub_id] = this.on_price.bind( this, sym );
  }
  get_symbols( res ) {
    this.symbols = [];
    let tab = document.getElementById( "prices" );
    for( let i = 0; i != res.result.length; ++i ) {
      let sym = res.result[i];
      let row = tab.insertRow(-1);
      let sym_td = document.createElement( 'TD' )
      sym_td.textContent = sym['symbol']
      sym_td.style['text-align'] = 'left';
      row.appendChild( sym_td );
      row.appendChild( document.createElement( 'TD' ) );
      row.appendChild( document.createElement( 'TD' ) );
      row.appendChild( document.createElement( 'TD' ) );
      row.appendChild( document.createElement( 'TD' ) );
      row.appendChild( document.createElement( 'TD' ) );
      let msg = {
        'method' : 'subscribe_price',
        'params' : {
          'symbol' : sym['symbol'],
          'price_type' : sym['price_type']
         }
       };
      sym.idx = i+1;
      this.send( msg, this.get_price.bind( this, sym ) );
    }
  }
}

let ws = null;
let px = null;
window.onload = function() {
  ws = new WebSocket('ws://localhost:8910')
  px = new Prices
  ws.onopen = function(e) {
    let n = document.getElementById('notify');
    n.textContent = 'connected';
    px.send( { 'method': 'get_symbol_list' },
             px.get_symbols.bind( px ) );
  };
  ws.onclose = function(e) {
    let n = document.getElementById('notify');
    n.textContent = 'disconnectd';
  }
  ws.onmessage = function(e) {
    let msg = JSON.parse(e.data);
    if ( 'id' in msg ) {
      px.on_response( msg );
    } else {
      px.on_notify( msg );
    }
  }
}
