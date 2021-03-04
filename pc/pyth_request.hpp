#pragma once

#include <pc/rpc_client.hpp>
#include <pc/prev_next.hpp>
#include <pc/dbl_list.hpp>
#include <oracle.h>

namespace pc
{
  class pyth_client;

  // pyth client api request
  class pyth_request : public prev_next<pyth_request>,
                       public error,
                       public rpc_sub
  {
  public:

    pyth_request();

    // pyth gateway server
    void set_pyth_client( pyth_client * );
    pyth_client *get_pyth_client() const;

    // rpc client interface
    void set_rpc_client( rpc_client * );
    rpc_client *get_rpc_client() const;

    // response callback
    void set_sub( rpc_sub * );
    rpc_sub *get_sub() const;

    // is status good to go
    virtual bool has_status() const;

    // has request finished
    virtual bool get_is_done() const;

    // submit requests as required
    virtual void submit() = 0;

  protected:

    template<class T> void on_error_sub( const std::string&, T * );
    template<class T> void on_response_sub( T * );

  private:
    pyth_client *svr_;
    rpc_client  *clnt_;
    rpc_sub     *cb_;
  };

  // create initial mapping acount
  class pyth_init_mapping : public pyth_request,
                            public rpc_sub_i<rpc::create_account>,
                            public rpc_sub_i<rpc::init_mapping>,
                            public rpc_sub_i<rpc::signature_subscribe>
  {
  public:
    void set_lamports( uint64_t );
    void submit() override;
    void on_response( rpc::create_account * ) override;
    void on_response( rpc::init_mapping * ) override;
    void on_response( rpc::signature_subscribe * ) override;
    bool get_is_done() const override;

  private:
    typedef enum {
      e_create_sent, e_create_sig,
      e_init_sent, e_init_sig, e_done, e_error
    } state_t;

    state_t                  st_;
    rpc::create_account      creq_[1];
    rpc::init_mapping        ireq_[1];
    rpc::signature_subscribe sig_[1];
  };

  // mapping account subsciption and update
  class pyth_get_mapping : public pyth_request,
                           public rpc_sub_i<rpc::get_account_info>,
                           public rpc_sub_i<rpc::account_subscribe>
  {
  public:
    void set_mapping_key( pub_key * );
    void submit() override;
    void on_response( rpc::get_account_info * ) override;
    void on_response( rpc::account_subscribe * ) override;
  private:
    typedef enum { e_new, e_init } state_t;

    template<class T> void update( T* );

    state_t  st_;
    pub_key *mkey_;
    size_t   num_;
    rpc::get_account_info  areq_[1];
    rpc::account_subscribe sreq_[1];
  };

  // add new symbol to mapping
  class pyth_add_symbol : public pyth_request,
                          public rpc_sub_i<rpc::create_account>,
                          public rpc_sub_i<rpc::signature_subscribe>,
                          public rpc_sub_i<rpc::add_symbol>
  {
  public:
    void set_symbol( const symbol& );
    void set_exponent( int32_t );
    void set_lamports( uint64_t funds );
    void on_response( rpc::create_account * ) override;
    void on_response( rpc::signature_subscribe * ) override;
    void on_response( rpc::add_symbol * ) override;
    bool has_status() const override;
    bool get_is_done() const override;
    void submit() override;

  private:
    typedef enum {
      e_create_sent, e_create_sig,
      e_add_sent, e_add_sig, e_done, e_error
    } state_t;

    state_t                  st_;
    key_pair                 akey_;
    rpc::create_account      areq_[1];
    rpc::add_symbol          sreq_[1];
    rpc::signature_subscribe sig_[1];
  };

  // add new price publisher to symbol account
  class pyth_add_publisher : public pyth_request,
                             public rpc_sub_i<rpc::signature_subscribe>,
                             public rpc_sub_i<rpc::add_publisher>
  {
  public:
    void set_symbol( const symbol& );
    void set_publisher( const pub_key& );
    void on_response( rpc::add_publisher * ) override;
    void on_response( rpc::signature_subscribe * ) override;
    bool has_status() const override;
    bool get_is_done() const override;
    void submit() override;

  private:
    typedef enum {
      e_add_sent, e_add_sig, e_done, e_error
    } state_t;

    state_t                  st_;
    key_pair                 akey_;
    rpc::add_publisher       req_[1];
    rpc::signature_subscribe sig_[1];
  };

  struct pc_price_node;

  // symbol price subscriber and publisher
  class pyth_symbol_price : public pyth_request,
                            public rpc_sub_i<rpc::get_account_info>,
                            public rpc_sub_i<rpc::account_subscribe>,
                            public rpc_sub_i<rpc::upd_price>,
                            public rpc_sub_i<rpc::signature_subscribe>
  {
  public:
    pyth_symbol_price( const symbol&, const pub_key& );
    void reset();
    bool has_publisher( const pub_key& );
    symbol  *get_symbol();
    pub_key *get_account();

    // submit new price update
    void upd_price( int64_t price, uint64_t conf );

    void submit() override;
    void on_response( rpc::get_account_info * ) override;
    void on_response( rpc::account_subscribe * ) override;
    void on_response( rpc::upd_price * ) override;
    void on_response( rpc::signature_subscribe * ) override;

  private:

    typedef enum {
      e_subscribe, e_publish, e_waitsig, e_error } state_t;

    template<class T> void update( T *res );

    bool init_publish();

    bool                     init_;
    bool                     pxchg_;
    state_t                  st_;
    symbol                   sym_;
    pub_key                  apub_;
    pc_price_node_t          pnd_;
    rpc::get_account_info    areq_[1];
    rpc::account_subscribe   sreq_[1];
    rpc::upd_price           preq_[1];
    rpc::signature_subscribe sig_[1];
  };

}
