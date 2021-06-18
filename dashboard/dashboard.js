'use strict';
class Prices
{
  constructor() {
    this.req   = [];
    this.sub   = [];
    this.reuse = [];
    this.fact  = [ 0, 1e-1, 1e-2, 1e-3, 1e-4, 1e-5, 1e-6, 1e-7, 1e-8, 1e-9, 1e-10 ];
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
  draw_cell( cell, txt, color ) {
    cell.textContent = txt;
    cell.style.color = color;
  }
  on_price( pxa, msg ) {
    let tab = document.getElementById( "prices" );
    let row = tab.rows[pxa.idx];
    let res = msg.params.result;
    let col = 0;
    let expo = -pxa.price_exponent;
    let px   = res.price * this.fact[expo];
    let conf = res.conf * this.fact[expo];
    let twap = res.twap * this.fact[expo];
    let twac = res.twac * this.fact[expo];
    let color = 'cornsilk';
    if ( res.status == 'unknown' ) {
      color = '#c0392b';
    }
    row.cells[col++].style.color = color;
    row.cells[col++].style.color = color;
    row.cells[col++].style.color = color;
    row.cells[col++].style.color = color;
    this.draw_cell( row.cells[col++], px.toFixed(expo), color );
    this.draw_cell( row.cells[col++], conf.toFixed(expo), color );
    this.draw_cell( row.cells[col++], twap.toFixed(expo), color );
    this.draw_cell( row.cells[col++], twac.toFixed(expo), color );
    this.draw_cell( row.cells[col++], res.status, color );
    this.draw_cell( row.cells[col++], res.valid_slot, color );
    this.draw_cell( row.cells[col++], res.pub_slot, color );
    row.cells[col++].style.color = color;
    row.cells[col++].style.color = color;
    row.cells[col++].style.color = color;
  }
  get_price( sym, msg ) {
    let sub_id = msg.result.subscription;
    while( sub_id < this.sub.length ) {
      this.sub.append( 0 )
    }
    this.sub[sub_id] = this.on_price.bind( this, sym );
  }
  get_title( att, title ) {
    let td = document.createElement( 'TD' )
    td.textContent = att[title]
    td.style['text-align'] = 'left';
    return td;
  }
  get_product_list( res ) {
    let tab = document.getElementById( "prices" );
    let k = 1;
    res.result.sort( function(a,b) {
      let asym = a['attr_dict']['symbol'];
      let bsym = b['attr_dict']['symbol'];
      return ( asym == bsym ? 0 : (asym > bsym ? 1: -1 ) );
    } );
    res.result.sort( function(a,b) {
      let atype = a['attr_dict']['asset_type'];
      let btype = b['attr_dict']['asset_type'];
      return ( atype == btype ? 0 : (atype > btype ? 1: -1 ) );
    } );
    for( let i = 0; i != res.result.length; ++i ) {
      let sym = res.result[i];
      let att = sym['attr_dict']
      for( let j=0; j != sym.price.length; ++j, ++k ) {
        let pxa = sym.price[j];
        let row = tab.insertRow(-1);
        row.appendChild( this.get_title( att, 'asset_type') );
        row.appendChild( this.get_title( att, 'country') );
        row.appendChild( this.get_title( att, 'symbol') );
        row.appendChild( this.get_title( pxa, 'price_type') );
        row.appendChild( document.createElement( 'TD' ) );
        row.appendChild( document.createElement( 'TD' ) );
        row.appendChild( document.createElement( 'TD' ) );
        row.appendChild( document.createElement( 'TD' ) );
        row.appendChild( document.createElement( 'TD' ) );
        row.appendChild( document.createElement( 'TD' ) );
        row.appendChild( document.createElement( 'TD' ) );
        row.appendChild( this.get_title( att, 'tenor') );
        row.appendChild( this.get_title( att, 'quote_currency') );
        row.appendChild( this.get_title( att, 'description') );
        let msg = {
          'method' : 'subscribe_price',
          'params' : {
            'account' : pxa['account']
           }
        };
        pxa.idx = k;
        this.send( msg, this.get_price.bind( this, pxa ) );
      }
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
    px.send( { 'method': 'get_product_list' },
             px.get_product_list.bind( px ) );
  };
  ws.onclose = function(e) {
    let n = document.getElementById('notify');
    n.textContent = 'disconnectd';
    n.style.backgroundColor = 'red';
    let hdr = document.getElementById('prices_div');
    hdr.style.backgroundColor = 'red';
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
