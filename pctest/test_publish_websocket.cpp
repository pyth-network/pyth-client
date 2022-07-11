#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include <ctime>
#include <iostream>
#include <memory>
#include <list>
#include <ctime>
#include <iostream>
#include <math.h>

#include <jcon/json_rpc_tcp_client.h>
#include <jcon/json_rpc_websocket_client.h>

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QUrl>
#include <QtGlobal>

/*
This file demonstrates how to use publish price updates to the Pyth Network using
the pythd websocket API described at https://docs.pyth.network/publish-data/pyth-client-websocket-api.

High-level flow:
- Call get_product_list_and_subscribe to fetch the product metadata, enabling us to associate the price account public 
  keys with the symbols we want to publish for.
- For each price account public key, call subscribe_price_sched to subscribe to the price update schedule
  for that price account. This will return a subscription ID.
- When we receive a notify_price_sched message, we should:
  - Look up the price account associated with the subscription ID.
  - If we have any new price information for that price account, send an update_price message to submit this
    to the network.

This example uses the jcon-cpp library, which depends on qt, but you can use any JSON-RPC protocol that
suits your stack.
*/

// This class is responsible for the communication with pythd over the websocket protocol
// described at https://docs.pyth.network/publish-data/pyth-client-websocket-api
// 
// It abstracts away any solana-specific details, such as public keys.
class pythd_websocket
{  
  public:
    // The status of a given symbol. Either "trading" or "halted".
    typedef std::string status_t;
    // The symbol of the product. For example "Crypto.BTC/USD" or "Equity.US.GOOG/USD".
    typedef std::string symbol_t;

    pythd_websocket( QObject* parent, std::string pythd_websocket_endpoint );

    // Submit a new price update for the given symbol, which will be sent to
    // pythd next time a notify_price_sched message for that symbol is received.
    void add_price_update( symbol_t symbol, int64_t price, uint64_t conf, status_t status );

  private:

    typedef struct {
      int64_t price;
      uint64_t conf;
      std::string status;
      time_t timestamp;
    } update_t;

    typedef struct {
      int64_t exponent;
    } account_metadata_t;

    typedef int64_t subscription_id_t;
    typedef std::string account_pubkey_t;

    // Mapping of account public keys to account metadata
    std::map<account_pubkey_t, account_metadata_t> account_to_metadata_;
    // Mapping of product symbols to price account public keys. 
    std::map<symbol_t, account_pubkey_t> symbol_to_account_;
    // Mapping of pythd subscription identifiers to price account public keys.
    std::map<subscription_id_t, account_pubkey_t> subscription_to_account_;
    // Mapping of price account public key to pythd subscription identifiers
    std::map<account_pubkey_t, subscription_id_t> account_to_subscription_;
    // Mapping of price account public keys to the updates we will send to pythd
    // next time we receive a notify_price_sched message with a subscription identifier
    // corresponding to the price account public key.
    std::map<account_pubkey_t, update_t> pending_updates_;

    // Websocket client to handle connection to pythd
    std::string pythd_websocket_endpoint_;
    jcon::JsonRpcWebSocketClient *rpc_client_;
    void connect( );

    // The duration between the price update being submitted and notify_price_sched being received
    // after which the update will be dropped.
    long update_staleness_threshold_secs_ = 3;

    // The pythd websocket API calls

    // Fetch the product list, and update the internal mapping of symbols to accounts.
    void get_product_list_and_subscribe( );

    // Send an update_price websocket message for the given price account.
    void update_price( account_pubkey_t account, int price, uint conf, status_t status );
    
    // Subscribe to the price update schedule for the given price account.
    void subscribe_price_sched( account_pubkey_t account );

    // Handler for notify_price_sched messages, indicating that any pending updates
    // for the price account associated with the given subscription id should be sent.
    void on_notify_price_sched( subscription_id_t subscription_id );

};

pythd_websocket::pythd_websocket( QObject* parent, std::string pythd_websocket_endpoint )
{
  this->pythd_websocket_endpoint_ = pythd_websocket_endpoint;
  this->rpc_client_ = new jcon::JsonRpcWebSocketClient(parent);

  // Set up the handler for notify_price_sched
  this->rpc_client_->enableReceiveNotification(true);
  QObject::connect(rpc_client_, &jcon::JsonRpcClient::notificationReceived, parent, [this](const QString& key, const QVariant& value){
    if (key == "notify_price_sched") {
      this->on_notify_price_sched( value.toMap()["subscription"].toInt() );
    }
  });

  // Continually check the connection and reconnect if dropped
  QTimer *timer = new QTimer( parent );
  QObject::connect(timer, &QTimer::timeout, parent, [this](){
    if ( !this->rpc_client_->isConnected() ) {
      // Reset the subscription state
      this->subscription_to_account_.clear();
      this->account_to_subscription_.clear();

      // Reconnect
      this->connect();
      this->get_product_list_and_subscribe();
    }
  });
  timer->start(1000);
}

void pythd_websocket::connect()
{
  this->rpc_client_->connectToServer(QUrl(QString::fromStdString(pythd_websocket_endpoint_)));
}

void pythd_websocket::get_product_list_and_subscribe( )
{
  auto req = this->rpc_client_->callAsync("get_product_list");

  req->connect(req.get(), &jcon::JsonRpcRequest::result, [this](const QVariant& result){
    // Loop over all the products
    auto products = result.toList();
    for (int i = 0; i < products.length(); i++) {

      auto product = products[i].toMap();

      // Extract the symbol, price account and exponent
      auto attr_dict = product["attr_dict"].toMap();
      symbol_t symbol = attr_dict["symbol"].toString().toStdString();
      auto price_account = product["price"].toList()[0].toMap();
      account_pubkey_t account = price_account["account"].toString().toStdString();
      int64_t exponent = price_account["price_exponent"].toInt();

      // If this is a new symbol, associate the symbol with the account
      if (this->symbol_to_account_.find(account) == this->symbol_to_account_.end() || this->symbol_to_account_[symbol] != account) {
        this->symbol_to_account_[symbol] = account;
      }

      // Update the account metadata
      this->account_to_metadata_[account] = account_metadata_t{
        exponent = exponent,
      };
      
      // If we don't already have a subscription for this account, subscribe to it
      if (account_to_subscription_.find(account) == account_to_subscription_.end()) {
        this->subscribe_price_sched(account);
      }
    }
  });

  req->connect(req.get(), &jcon::JsonRpcRequest::error, [](int code, const QString& message) {
    std::cout << "error sending get_product_list (" << code << ") " << message.toStdString() << std::endl;
  });
}

void pythd_websocket::subscribe_price_sched( account_pubkey_t account )
{
  auto req = this->rpc_client_->callAsyncNamedParams("subscribe_price_sched",
    QVariantMap{
      {"account", QString::fromStdString(account)},
      });

  req->connect(req.get(), &jcon::JsonRpcRequest::error, [](int code, const QString& message) {
    std::cout << "error sending subscribe_price_sched (" << code << ") " << message.toStdString() << std::endl;
  });

  req->connect(req.get(), &jcon::JsonRpcRequest::result, [this, account](const QVariant& result){
    auto subscription_id = result.toMap()["subscription"].toInt();
    subscription_to_account_[subscription_id] = account; 
    account_to_subscription_[account] = subscription_id;
    std::cout << "received subscription id " << subscription_id << " for account " << account << std::endl;
  });
}

void pythd_websocket::update_price( account_pubkey_t account, int price, uint conf, status_t status )
{
  // Scale the price and confidence by the exponent
  int64_t exponent = (-1) * this->account_to_metadata_[account].exponent;
  double scaled_price = price * pow(10, exponent);
  double scaled_conf = conf * pow(10, exponent);

  auto req = this->rpc_client_->callAsyncNamedParams("update_price",
    QVariantMap{
      {"account", QString::fromStdString(account)},
      {"price", scaled_price},
      {"conf", scaled_conf},
      {"status", QString::fromStdString(status)}
      });

  req->connect(req.get(), &jcon::JsonRpcRequest::error, [account](int code, const QString& message) {
    std::cout << "error sending update_price  (" << code << ") " << message.toStdString() << std::endl;
  });
}

void pythd_websocket::on_notify_price_sched( subscription_id_t subscription_id )
{
  // Fetch the account associated with the subscription
  if (this->subscription_to_account_.find(subscription_id) == this->subscription_to_account_.end()) {
    return;
  }
  account_pubkey_t account = subscription_to_account_[subscription_id];

  // Fetch any price update we have for this account
  if (this->pending_updates_.find(account) == this->pending_updates_.end()) {
    return;
  }
  update_t update = pending_updates_[account];
  this->pending_updates_.erase(account);

  // Check that the update is not stale
  if ( (std::time(nullptr) - update.timestamp) > update_staleness_threshold_secs_) {
    return;
  }

  // Send the price update
  update_price( account, update.price, update.conf, update.status );
}

void pythd_websocket::add_price_update( symbol_t symbol, int64_t price, uint64_t conf, status_t status ) {
  if (this->symbol_to_account_.find(symbol) == this->symbol_to_account_.end()) {
    return;
  }
  account_pubkey_t account = symbol_to_account_[symbol];

  pending_updates_[account] = update_t{
    price: price,
    conf: conf,
    status: status,
    timestamp: std::time(nullptr),
  };
}

class test_publish;

// The test_publish class is responsible for computing the next price and confidence values
// for the symbols and submitting these to its pythd_websocket publisher.
class test_publish
{  
  public:
    test_publish( QObject* parent, std::vector<std::string> symbols_to_publish, std::string pythd_websocket_endpoint );

  private:
    pythd_websocket *pythd_websocket_;
    std::vector<std::string> symbols_to_publish_;

    void update_symbols();
};

test_publish::test_publish(
  QObject* parent,
  std::vector<std::string> symbols_to_publish,
  std::string pythd_websocket_endpoint )
{
    pythd_websocket_ = new pythd_websocket( parent, pythd_websocket_endpoint );
    this->symbols_to_publish_.insert(this->symbols_to_publish_.end(), symbols_to_publish.begin(), symbols_to_publish.end());

    // Continually generate new values for the symbols
    QTimer *timer = new QTimer( parent );
    QObject::connect(timer, &QTimer::timeout, parent, [this](){
      this->update_symbols();
    });
    timer->start(1000);
}

void test_publish::update_symbols() {

  // Send a random price update for each symbol
  for (size_t i = 0; i < symbols_to_publish_.size(); i++) {
    int64_t next_price = std::rand();
    uint64_t next_conf = (uint64_t) std::rand();
  
    this->pythd_websocket_->add_price_update(
      this->symbols_to_publish_[i], next_price, next_conf, "trading" );
  }

}

int main( int argc, char* argv[] )
{
  std::string pythd_websocket_endpoint =  "ws://127.0.0.1:8910";

  QCoreApplication app(argc, argv);
  std::vector<std::string> symbols_to_publish = { "Crypto.BTC/USD", "Crypto.ETH/USD" };
  new test_publish( &app, symbols_to_publish, pythd_websocket_endpoint );

  app.exec();
}
