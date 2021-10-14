#pragma once

#include <pc/rpc_client.hpp>
#include <pc/dbl_list.hpp>
#include <pc/attr_id.hpp>
#include <pc/pub_stats.hpp>
#include <oracle/oracle.h>

namespace pc
{
  class manager;
  class request;
  class product;
  class price;

  // pyth request subscriber
  class request_sub
  {
  public:
    virtual ~request_sub();
  };

  // rpc subscription callback for request type T
  template<class T>
  class request_sub_i
  {
  public:
    virtual ~request_sub_i() {}
    virtual void on_response( T *, uint64_t ) = 0;
  };

  // individual request subscription
  struct request_node : public prev_next<request_node>
  {
    request_node( request_sub*, request*, uint64_t idx );
    request_sub *sub_;
    request     *req_;
    uint64_t     idx_;
  };

  // map subscription to multiple requests
  class request_sub_set
  {
  public:
    request_sub_set( request_sub * );
    ~request_sub_set();
    uint64_t add( request * );
    bool del( uint64_t );
    void teardown();
  private:
    typedef std::vector<request_node*> sub_vec_t;
    typedef std::vector<uint64_t>      sub_idx_t;
    request_sub  *sptr_;
    sub_vec_t  svec_;
    sub_idx_t  rvec_;
    uint64_t   sidx_;
  };

  // pyth manager api request
  class request : public prev_next<request>,
                  public error,
                  public rpc_sub,
                  public rpc_sub_i<rpc::account_update>
  {
  public:

    typedef prev_next<request> prev_next_t;

    request();

    // pyth gateway server
    void set_manager( manager * );
    manager *get_manager() const;

    // rpc manager interface
    void set_rpc_client( rpc_client * );
    rpc_client *get_rpc_client() const;

    // list node for tracking all requests
    prev_next_t *get_request();

    // response callbacks
    void add_sub( request_node * );
    void del_sub( request_node * );

    // is status good to go
    virtual bool get_is_ready();

    // has request finished
    virtual bool get_is_done() const;

    // submit requests as required
    virtual void submit() = 0;

    // is in submission queue
    void set_is_submit( bool );
    bool get_is_submit() const;

    // has received account update
    void set_is_recv( bool );
    bool get_is_recv() const;
    void on_response( rpc::account_update * ) override;

  protected:

    template<class T> void on_error_sub( const std::string&, T * );
    template<class T> void on_response_sub( T * );

  private:

    typedef dbl_list<request_node> node_list_t;

    manager    *svr_;
    rpc_client *clnt_;
    prev_next_t nd_[1];
    node_list_t slist_;
    bool        is_submit_;
    bool        is_recv_;
  };

  // mapping account subsciption and update
  class get_mapping : public request,
                      public rpc_sub_i<rpc::get_account_info>
  {
  public:
    get_mapping();
    void set_mapping_key( const pub_key& );
    pub_key *get_mapping_key();
    uint32_t get_num_symbols() const;
    bool     get_is_full() const;

  public:
    void reset();
    void submit() override;
    void on_response( rpc::get_account_info * ) override;
    void on_response( rpc::account_update * ) override;
  private:
    typedef enum { e_new, e_init } state_t;

    template<class T> void update( T* );

    state_t  st_;
    pub_key  mkey_;
    uint32_t num_sym_;
    rpc::get_account_info areq_[1];
  };

  // product symbol and other reference-data attributes
  class product : public request,
                  public attr_dict,
                  public rpc_sub_i<rpc::get_account_info>
  {
  public:
    // product account number
    pub_key *get_account();
    const pub_key *get_account() const;

    // symbol from attr_dict
    str get_symbol();

    // iterate through associated price accounts (quotes) associated
    // with this product
    unsigned get_num_price() const;
    price *get_price( unsigned i ) const;
    price *get_price( price_type ) const;

    // output full set of data to json writer
    void dump_json( json_wtr& wtr ) const;

  public:

    product( const pub_key& );
    virtual ~product();
    void reset();
    void submit() override;
    void on_response( rpc::get_account_info * ) override;
    void on_response( rpc::account_update * ) override;
    bool get_is_done() const override;
    void add_price( price * );

  private:
    typedef enum { e_subscribe, e_done, e_error } state_t;
    typedef std::vector<price*> prices_t;

    template<class T> void update( T *res );

    pub_key                acc_;
    prices_t               pvec_;
    state_t                st_;
    rpc::get_account_info  areq_[1];
  };

  // price submission schedule
  class price_sched : public request
  {
  public:
    price_sched( price * );

    // get associated symbol price
    price *get_price() const;

  public:
    static const uint64_t fraction = 997UL;

    bool get_is_ready() override;
    void submit() override;
    uint64_t get_hash() const;
    void schedule();

  private:
    price   *ptr_;
    uint64_t shash_;
  };

  // price account re-initialized
  class price_init : public request
  {
  public:
    price_init( price * );

    // get associated symbol price
    price *get_price() const;

    void submit() override;
  private:
    price *ptr_;
  };

  // price subscriber and publisher
  class price : public request,
                public pub_stats,
                public rpc_sub_i<rpc::get_account_info>,
                public rpc_sub_i<rpc::upd_price>
  {
  public:

    price( const pub_key&, product *prod );
    virtual ~price();

    // corresponding product definition
    product *get_product() const;

    // convenience wrapper equiv to: get_product()->get_symbol()
    str get_symbol();

    // convenience wrapper equiv to: get_product()->get_sttr()
    bool get_attr( attr_id, str& ) const;

    // is current publisher authorized to publish on this symbol
    bool has_publisher();

    // is publisher authorized to publish on this symbol
    bool has_publisher( const pub_key& );

    // ready to publish (i.e. not waiting for confirmation)
    bool get_is_ready_publish() const;

    // submit new price update and update aggregate
    // will fail with false if in error (check get_is_err() )
    // or because symbol is not ready to publish (get_is_ready_publish())
    // or because publisher does not have permission (has_publisher())
    bool update( int64_t price, uint64_t conf, symbol_status );

    // update aggregate price only
    bool update();

    // are there any update transactions which have not yet been acked
    bool has_unacked_updates() const;

    // get and activate price schedule subscription
    price_sched *get_sched();

    // various accessors
    pub_key       *get_account();
    const pub_key *get_account() const;
    price_type     get_price_type() const;
    int64_t        get_price_exponent() const;
    uint32_t       get_version() const;
    int64_t        get_price() const;
    uint64_t       get_conf() const;
    symbol_status  get_status() const;
    uint32_t       get_num_qt() const;
    uint64_t       get_lamports() const;
    int64_t        get_twap() const;
    uint64_t       get_twac() const;
    uint64_t       get_prev_slot() const;
    int64_t        get_prev_price() const;
    uint64_t       get_prev_conf() const;

    // get publishers
    unsigned get_num_publisher() const;
    int64_t  get_publisher_price( unsigned ) const;
    uint64_t get_publisher_conf( unsigned ) const;
    uint64_t get_publisher_slot( unsigned ) const;
    symbol_status  get_publisher_status( unsigned ) const;
    const pub_key *get_publisher( unsigned ) const;

    // slot that corresponds to the prices used to compile the last
    // published aggregate
    uint64_t      get_valid_slot() const;

    // slot of last aggregate price
    uint64_t      get_pub_slot() const;

    // output full set of data to json writer
    void dump_json( json_wtr& wtr ) const;

  public:

    void set_price_type( price_type );
    void set_version( uint32_t );
    void set_price( int64_t );
    void set_conf( int64_t );
    void set_symbol_status( symbol_status );
    void set_product( product * );

    void reset();
    void unsubscribe();
    void submit() override;
    void on_response( rpc::upd_price * ) override;
    void on_response( rpc::get_account_info * ) override;
    void on_response( rpc::account_update * ) override;
    bool get_is_done() const override;

  private:

    typedef enum {
      e_subscribe, e_sent_subscribe, e_publish, e_error } state_t;

    typedef std::vector<std::pair<std::string,int64_t>> txid_vec_t;

    template<class T> void update( T *res );

    bool init_publish();
    void init_subscribe();
    void log_update( const char *title );
    void update_pub();
    bool update( int64_t price, uint64_t conf, symbol_status, bool aggr );

    bool                   init_;
    bool                   isched_;
    state_t                st_;
    uint32_t               pub_idx_;
    pub_key                apub_;
    uint64_t               lamports_;
    uint64_t               pub_slot_;
    product               *prod_;
    price_sched            sched_;
    price_init             pinit_;
    rpc::get_account_info  areq_[1];
    rpc::upd_price         preq_[1];
    pc_price_t            *pptr_;
    txid_vec_t             tvec_;
  };

  template<class T>
  void request::on_response_sub( T *req )
  {
    for( request_node *sptr = slist_.first(); sptr; ) {
      request_node *nxt = sptr->get_next();
      request_sub_i<T> *iptr = dynamic_cast<request_sub_i<T>*>( sptr->sub_ );
      if ( iptr ) {
        iptr->on_response( req, sptr->idx_ );
      }
      sptr = nxt;
    }
  }

  template<class T>
  void request::on_error_sub( const std::string& emsg, T *req )
  {
    set_err_msg( emsg );
    on_response_sub( req );
  }

}
