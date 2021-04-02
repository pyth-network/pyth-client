#pragma once

#include <pc/rpc_client.hpp>
#include <pc/dbl_list.hpp>
#include <oracle/oracle.h>

namespace pc
{
  class manager;
  class request;

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
                  public rpc_sub
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
                      public rpc_sub_i<rpc::get_account_info>,
                      public rpc_sub_i<rpc::account_subscribe>
  {
  public:
    get_mapping();
    void set_mapping_key( const pub_key& );
    pub_key *get_mapping_key();

  public:
    void reset();
    void submit() override;
    void on_response( rpc::get_account_info * ) override;
    void on_response( rpc::account_subscribe * ) override;
  private:
    typedef enum { e_new, e_init } state_t;

    void init( pc_map_table_t * );
    template<class T> void update( T* );

    state_t  st_;
    pub_key  mkey_;
    rpc::get_account_info  areq_[1];
    rpc::account_subscribe sreq_[1];
  };

  // add new symbol to mapping
  class add_symbol : public request,
                     public rpc_sub_i<rpc::create_account>,
                     public rpc_sub_i<rpc::signature_subscribe>,
                     public rpc_sub_i<rpc::add_symbol>
  {
  public:
    add_symbol();
    void set_symbol( const symbol& );
    void set_exponent( int32_t );
    void set_lamports( uint64_t funds );
    void set_price_type( price_type );
    void set_commitment( commitment );
    bool get_is_done() const override;

  public:
    void on_response( rpc::create_account * ) override;
    void on_response( rpc::signature_subscribe * ) override;
    void on_response( rpc::add_symbol * ) override;
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
    rpc::create_account      areq_[1];
    rpc::add_symbol          sreq_[1];
    rpc::signature_subscribe sig_[1];
  };

  // add new price publisher to symbol account
  class add_publisher : public request,
                        public rpc_sub_i<rpc::signature_subscribe>,
                        public rpc_sub_i<rpc::add_publisher>
  {
  public:
    add_publisher();
    void set_symbol( const symbol& );
    void set_publisher( const pub_key& );
    void set_price_type( price_type );
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
    key_pair                 akey_;
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
    void set_symbol( const symbol& );
    void set_publisher( const pub_key& );
    void set_price_type( price_type );
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
    key_pair                 akey_;
    rpc::del_publisher       req_[1];
    rpc::signature_subscribe sig_[1];
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

  class price;

  // price submission schedule
  class price_sched : public request
  {
  public:
    price_sched( price * );

    // get associated symbol price
    price *get_price() const;

  public:
    static const uint64_t fraction = 997UL;
    static const uint64_t period   = 1097UL;

    bool get_is_ready() override;
    void submit() override;
    uint64_t get_hash() const;
    void schedule();

  private:
    price   *ptr_;
    uint64_t shash_;
  };

  // price subscriber and publisher
  class price : public request,
                public rpc_sub_i<rpc::get_account_info>,
                public rpc_sub_i<rpc::account_subscribe>,
                public rpc_sub_i<rpc::upd_price>,
                public rpc_sub_i<rpc::signature_subscribe>
  {
  public:

    price( const pub_key& );

    // is publisher authorized to publish on this symbol
    bool has_publisher( const pub_key& );

    // ready to publish (i.e. not waiting for confirmation)
    bool get_is_ready() const;

    // submit new price update
    // will fail with false if in error (check get_is_err() )
    // or because symbol is not ready to publish (check get_is_ready())
    bool update( int64_t price, uint64_t conf, symbol_status );

    // get and activate price schedule subscription
    price_sched *get_sched();

    // various accessors
    symbol       *get_symbol();
    pub_key      *get_account();
    price_type    get_price_type() const;
    int64_t       get_price_exponent() const;
    uint32_t      get_version() const;
    int64_t       get_price() const;
    uint64_t      get_conf() const;
    symbol_status get_status() const;
    uint64_t      get_lamports() const;

    // get publishers
    unsigned get_num_publisher() const;
    int64_t  get_publisher_price( unsigned ) const;
    uint64_t get_publisher_conf( unsigned ) const;
    const pub_key *get_publisher( unsigned ) const;

    // slot that corresponds to the prices used to compile the last
    // published aggregate
    uint64_t      get_valid_slot() const;

    // slot of last aggregate price
    uint64_t      get_pub_slot() const;

    // slot of last confirmed published price
    uint64_t      get_last_slot() const;

  public:

    void set_symbol( const symbol& );
    void set_price_type( price_type );
    void set_version( uint32_t );
    void set_price( int64_t );
    void set_conf( int64_t );
    void set_symbol_status( symbol_status );

    void reset();
    void submit() override;
    void on_response( rpc::get_account_info * ) override;
    void on_response( rpc::account_subscribe * ) override;
    void on_response( rpc::upd_price * ) override;
    void on_response( rpc::signature_subscribe * ) override;

  private:

    typedef enum {
      e_subscribe, e_sent_subscribe,
      e_publish, e_pend_publish, e_sent_publish, e_sent_sig,
      e_error } state_t;

    template<class T> void update( T *res );

    bool init_publish();
    void init_subscribe( pc_price_t * );

    bool                   init_;
    bool                   isched_;
    state_t                st_;
    symbol                 sym_;
    pub_key                apub_;
    price_type             ptype_;
    symbol_status          sym_st_;
    uint32_t               version_;
    int64_t                apx_;
    uint64_t               aconf_;
    uint64_t               valid_slot_;
    uint64_t               pub_slot_;
    uint64_t               lamports_;
    int32_t                aexpo_;
    uint32_t               cnum_;
    pub_key               *pkey_;
    price_sched            sched_;
    pc_pub_key_t           cpub_[PC_COMP_SIZE];
    int64_t                cprice_[PC_COMP_SIZE];
    uint64_t               cconf_[PC_COMP_SIZE];
    rpc::get_account_info  areq_[1];
    rpc::account_subscribe sreq_[1];
    rpc::upd_price         preq_[1];
    rpc::signature_subscribe sig_[1];
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
