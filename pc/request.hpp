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
                  public rpc_sub_i<rpc::program_subscribe>
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
    void on_response( rpc::program_subscribe * ) override;

  protected:

    template<class T> void on_error_sub( const std::string&, T * );
    template<class T> void on_response_sub( T * );

  private:

    typedef dbl_list<request_node> node_list_t;

    manager    *svr_;
    rpc_client *clnt_;
    rpc_sub    *cb_;
    prev_next_t nd_[1];
    node_list_t slist_;
    bool        is_submit_;
    bool        is_recv_;
  };

  // create initial mapping acount
  class init_mapping : public request,
                       public rpc_sub_i<rpc::create_account>,
                       public rpc_sub_i<rpc::init_mapping>,
                       public rpc_sub_i<rpc::signature_subscribe>
  {
  public:
    init_mapping();
    void set_lamports( uint64_t );
    void set_commitment( commitment );
    bool get_is_done() const override;

  public:
    void submit() override;
    void on_response( rpc::create_account * ) override;
    void on_response( rpc::init_mapping * ) override;
    void on_response( rpc::signature_subscribe * ) override;

  private:
    typedef enum {
      e_create_sent, e_create_sig,
      e_init_sent, e_init_sig, e_done, e_error
    } state_t;

    state_t                  st_;
    commitment               cmt_;
    rpc::create_account      creq_[1];
    rpc::init_mapping        ireq_[1];
    rpc::signature_subscribe sig_[1];
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
    void on_response( rpc::program_subscribe * ) override;
  private:
    typedef enum { e_new, e_init } state_t;

    template<class T> void update( T* );

    state_t  st_;
    pub_key  mkey_;
    uint32_t num_sym_;
    rpc::get_account_info areq_[1];
  };

  // add new mapping acount
  class add_mapping : public request,
                      public rpc_sub_i<rpc::create_account>,
                      public rpc_sub_i<rpc::add_mapping>,
                      public rpc_sub_i<rpc::signature_subscribe>
  {
  public:
    add_mapping();
    void set_lamports( uint64_t );
    void set_commitment( commitment );
    bool get_is_done() const override;

  public:
    void submit() override;
    bool get_is_ready() override;
    void on_response( rpc::create_account * ) override;
    void on_response( rpc::add_mapping * ) override;
    void on_response( rpc::signature_subscribe * ) override;

  private:
    typedef enum {
      e_create_sent, e_create_sig,
      e_init_sent, e_init_sig, e_done, e_error
    } state_t;

    state_t                  st_;
    commitment               cmt_;
    key_pair                 akey_;
    key_pair                 mkey_;
    rpc::create_account      areq_[1];
    rpc::add_mapping         mreq_[1];
    rpc::signature_subscribe sig_[1];
  };

  // add new product to mapping
  class add_product : public request,
                      public rpc_sub_i<rpc::create_account>,
                      public rpc_sub_i<rpc::signature_subscribe>,
                      public rpc_sub_i<rpc::add_product>
  {
  public:
    add_product();
    void set_lamports( uint64_t funds );
    void set_commitment( commitment );
    bool get_is_done() const override;
    key_pair *get_account();

  public:
    void on_response( rpc::create_account * ) override;
    void on_response( rpc::signature_subscribe * ) override;
    void on_response( rpc::add_product * ) override;
    bool get_is_ready() override;
    void submit() override;

  private:
    typedef enum {
      e_create_sent, e_create_sig,
      e_add_sent, e_add_sig, e_done, e_error
    } state_t;

    state_t                  st_;
    commitment               cmt_;
    key_pair                 akey_;
    key_pair                 skey_;
    key_pair                 mkey_;
    rpc::create_account      areq_[1];
    rpc::add_product         sreq_[1];
    rpc::signature_subscribe sig_[1];
  };

  // update product attribute data
  class upd_product : public request,
                      public rpc_sub_i<rpc::signature_subscribe>,
                      public rpc_sub_i<rpc::upd_product>
  {
  public:
    upd_product();
    void set_commitment( commitment );
    void set_attr_dict( attr_dict * );
    void set_product( product * );
    bool get_is_done() const override;

  public:
    void on_response( rpc::signature_subscribe * ) override;
    void on_response( rpc::upd_product * ) override;
    bool get_is_ready() override;
    void submit() override;
    void reset();

  private:
    typedef enum {
      e_sent, e_sig, e_done, e_error
    } state_t;

    state_t                  st_;
    commitment               cmt_;
    product                 *prod_;
    key_pair                 skey_;
    rpc::upd_product         sreq_[1];
    rpc::signature_subscribe sig_[1];
  };


  // add new price account to product
  class add_price : public request,
                    public rpc_sub_i<rpc::create_account>,
                    public rpc_sub_i<rpc::signature_subscribe>,
                    public rpc_sub_i<rpc::add_price>
  {
  public:
    add_price();
    void set_exponent( int32_t );
    void set_lamports( uint64_t funds );
    void set_price_type( price_type );
    void set_product( product * );
    void set_commitment( commitment );
    bool get_is_done() const override;
    key_pair *get_account();

  public:
    void on_response( rpc::create_account * ) override;
    void on_response( rpc::signature_subscribe * ) override;
    void on_response( rpc::add_price * ) override;
    bool get_is_ready() override;
    void submit() override;

  private:
    typedef enum {
      e_create_sent, e_create_sig,
      e_add_sent, e_add_sig, e_done, e_error
    } state_t;

    state_t                  st_;
    commitment               cmt_;
    product                 *prod_;
    key_pair                 akey_;
    key_pair                 skey_;
    key_pair                 mkey_;
    rpc::create_account      areq_[1];
    rpc::add_price           sreq_[1];
    rpc::signature_subscribe sig_[1];
  };

  // (re)initialize price account to reflect exponent change
  class init_price : public request,
                     public rpc_sub_i<rpc::signature_subscribe>,
                     public rpc_sub_i<rpc::init_price>
  {
  public:
    init_price();
    void set_exponent( int32_t );
    void set_price( price * );
    void set_commitment( commitment );
    bool get_is_done() const override;

  public:
    void on_response( rpc::signature_subscribe * ) override;
    void on_response( rpc::init_price * ) override;
    bool get_is_ready() override;
    void submit() override;

  private:
    typedef enum {
      e_init_sent, e_init_sig, e_done, e_error
    } state_t;

    state_t                  st_;
    commitment               cmt_;
    price                   *price_;
    key_pair                 key_;
    rpc::init_price          req_[1];
    rpc::signature_subscribe sig_[1];
  };

  // add new price publisher to symbol account
  class add_publisher : public request,
                        public rpc_sub_i<rpc::signature_subscribe>,
                        public rpc_sub_i<rpc::add_publisher>
  {
  public:
    add_publisher();
    void set_price( price * );
    void set_publisher( const pub_key& );
    void set_commitment( commitment );
    bool get_is_done() const override;

  public:
    void on_response( rpc::add_publisher * ) override;
    void on_response( rpc::signature_subscribe * ) override;
    bool get_is_ready() override;
    void submit() override;

  private:
    typedef enum {
      e_add_sent, e_add_sig, e_done, e_error
    } state_t;

    state_t                  st_;
    commitment               cmt_;
    price                   *px_;
    key_pair                 akey_;
    key_pair                 mkey_;
    rpc::add_publisher       req_[1];
    rpc::signature_subscribe sig_[1];
  };

  // remove price publisher from symbol account
  class del_publisher : public request,
                        public rpc_sub_i<rpc::signature_subscribe>,
                        public rpc_sub_i<rpc::del_publisher>
  {
  public:
    del_publisher();
    void set_price( price * );
    void set_publisher( const pub_key& );
    void set_commitment( commitment );
    bool get_is_done() const override;

  public:
    void on_response( rpc::del_publisher * ) override;
    void on_response( rpc::signature_subscribe * ) override;
    bool get_is_ready() override;
    void submit() override;

  private:
    typedef enum {
      e_add_sent, e_add_sig, e_done, e_error
    } state_t;

    state_t                  st_;
    commitment               cmt_;
    price                   *px_;
    key_pair                 akey_;
    rpc::del_publisher       req_[1];
    rpc::signature_subscribe sig_[1];
  };

  // initialize test account
  class init_test : public request,
                    public rpc_sub_i<rpc::create_account>,
                    public rpc_sub_i<rpc::init_test>,
                    public rpc_sub_i<rpc::signature_subscribe>
  {
  public:
    init_test();
    void set_lamports( uint64_t );
    bool get_is_done() const override;
    key_pair *get_account();

  public:
    void submit() override;
    void on_response( rpc::create_account * ) override;
    void on_response( rpc::init_test * ) override;
    void on_response( rpc::signature_subscribe * ) override;

  private:
    typedef enum {
      e_create_sent, e_create_sig,
      e_init_sent, e_init_sig, e_done, e_error
    } state_t;

    state_t                  st_;
    key_pair                 tkey_;
    rpc::create_account      creq_[1];
    rpc::init_test           ireq_[1];
    rpc::signature_subscribe sig_[1];
  };

  // run aggregate price test
  class upd_test : public request,
                   public rpc_sub_i<rpc::upd_test>,
                   public rpc_sub_i<rpc::account_subscribe>
  {
  public:
    void set_test_key( const std::string& );
    bool init_from_file( const std::string& );
    bool get_is_done() const override;

  public:

    void submit() override;
    void on_response( rpc::upd_test * ) override;
    void on_response( rpc::account_subscribe * ) override;

  private:
    typedef enum {
      e_upd_sent, e_upd_sig, e_get_sent, e_done, e_error
    } state_t;

    state_t                  st_;
    pub_key                  tpub_;
    key_pair                 tkey_;
    rpc::create_account      creq_[1];
    rpc::upd_test            ureq_[1];
    rpc::account_subscribe   areq_[1];
  };


  // transfer SOL from publisher to other account
  class transfer : public request,
                   public rpc_sub_i<rpc::transfer>,
                   public rpc_sub_i<rpc::signature_subscribe>
  {
  public:
    transfer();
    void set_receiver( pub_key * );
    void set_lamports( uint64_t funds );
    void set_commitment( commitment );

  public:
    void on_response( rpc::transfer *msg ) override;
    void on_response( rpc::signature_subscribe * ) override;
    bool get_is_done() const override;
    void submit() override;

  private:
    typedef enum { e_sent, e_sig, e_done, e_error } state_t;

    state_t                  st_;
    commitment               cmt_;
    key_pair                 akey_;
    key_pair                 skey_;
    rpc::transfer            req_[1];
    rpc::signature_subscribe sig_[1];
  };

  // get account balance
  class balance : public request,
                  public rpc_sub_i<rpc::get_account_info>
  {
  public:
    balance();
    void set_pub_key( pub_key * );
    uint64_t get_lamports() const;

  public:
    void submit() override;
    void on_response( rpc::get_account_info * ) override;
    bool get_is_done() const override;

  private:
    typedef enum { e_sent, e_done, e_error } state_t;
    state_t  st_;
    pub_key *pub_;
    rpc::get_account_info req_[1];
  };

  // get transactions in single block
  class get_block : public request,
                    public rpc_sub_i<rpc::get_block>
  {
  public:
    get_block();
    void set_slot( uint64_t slot );
    void set_commitment( commitment cmt );
  public:
    void submit() override;
    void on_response( rpc::get_block * ) override;
    bool get_is_done() const override;
  protected:
    typedef enum { e_sent, e_done, e_error } state_t;
    state_t st_;
    rpc::get_block req_[1];
  };

  // get minimum balance for rent exemption
  class get_minimum_balance_rent_exemption : public request,
    public rpc_sub_i<rpc::get_minimum_balance_rent_exemption>
  {
  public:
    get_minimum_balance_rent_exemption();
    void set_size( size_t );
    uint64_t get_lamports() const;

  public:
    void submit() override;
    void on_response( rpc::get_minimum_balance_rent_exemption* ) override;
    bool get_is_done() const override;

  private:
    typedef enum { e_sent, e_done, e_error } state_t;
    state_t  st_;
    rpc::get_minimum_balance_rent_exemption req_[1];
  };

  // product symbol and other reference-data attributes
  class product : public request,
                  public attr_dict,
                  public rpc_sub_i<rpc::get_account_info>
  {
  public:
    // product account number
    pub_key *get_account();

    // symbol from attr_dict
    str get_symbol();

    // iterate through associated price accounts (quotes) associated
    // with this product
    unsigned get_num_price() const;
    price *get_price( unsigned i ) const;
    price *get_price( price_type ) const;

  public:

    product( const pub_key& );
    virtual ~product();
    void reset();
    void submit() override;
    void on_response( rpc::get_account_info * ) override;
    void on_response( rpc::program_subscribe * ) override;
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

  // price account rei-initialized
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
                public rpc_sub_i<rpc::get_account_info>
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

    // get and activate price schedule subscription
    price_sched *get_sched();

    // various accessors
    pub_key      *get_account();
    price_type    get_price_type() const;
    int64_t       get_price_exponent() const;
    uint32_t      get_version() const;
    int64_t       get_price() const;
    uint64_t      get_conf() const;
    symbol_status get_status() const;
    uint64_t      get_lamports() const;
    int64_t       get_twap() const;
    uint64_t      get_twac() const;
    uint64_t      get_prev_slot() const;
    int64_t       get_prev_price() const;
    uint64_t      get_prev_conf() const;

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
    void on_response( rpc::get_account_info * ) override;
    void on_response( rpc::program_subscribe * ) override;
    bool get_is_done() const override;

  private:

    typedef enum {
      e_subscribe, e_sent_subscribe, e_publish, e_error } state_t;

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
