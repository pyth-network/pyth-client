#pragma once

#include "admin_rpc_client.hpp"
#include <pc/request.hpp>

namespace pc
{

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

}
