#pragma once

#include <vector>
#include <stdlib.h>

namespace pc
{

  // hash map based on type trait T
  template<class T>
  class hash_map
  {
  public:

    typedef typename T::idx_t    idx_t;
    typedef typename T::key_t    key_t;
    typedef typename T::keyref_t keyref_t;
    typedef typename T::val_t    val_t;
    typedef typename T::hash_t   hash_t;
    typedef idx_t    *iter_t;
    static const size_t hsize_ = T::hsize_;

    hash_map( hash_t hfn = hash_t() );

    iter_t find( keyref_t );
    iter_t add( keyref_t );
    const val_t &const_ref( iter_t ) const;
    val_t &ref( iter_t );
    val_t  obj( iter_t );
    void   del( iter_t );
    size_t size() const;
    void   clear();

  private:

    struct node {
      idx_t nxt_;
      key_t key_;
      val_t val_;
    };

    typedef std::vector<node> node_vec_t;

    node_vec_t nvec_;
    idx_t      htab_[hsize_];
    idx_t      rhd_;
    hash_t     hfn_;
    size_t     nval_;
  };

  template<class T>
  hash_map<T>::hash_map( hash_t hfn )
  : nvec_( 1 ),
    rhd_( 0 ),
    hfn_( hfn ),
    nval_( 0 )
  {
    __builtin_memset( htab_, 0 , sizeof( htab_ ) );
  }

  template<class T>
  typename hash_map<T>::iter_t hash_map<T>::find( keyref_t k )
  {
    idx_t i = hfn_( k )%hsize_;
    for( iter_t p = &htab_[i]; *p; ) {
      node& nd = nvec_[*p];
      if ( k == nd.key_ ) {
        return p;
      }
      p = &nvec_[*p].nxt_;
    }
    return nullptr;
  }

  template<class T>
  void hash_map<T>::del( iter_t i )
  {
    idx_t it = *i;
    node& nd = nvec_[it];
    *i = nd.nxt_;
    nd.nxt_ = rhd_;
    rhd_ = it;
    --nval_;
  }

  template<class T>
  typename hash_map<T>::iter_t hash_map<T>::add( keyref_t k )
  {
    idx_t res = rhd_;
    if ( res ) {
      rhd_ = nvec_[rhd_].nxt_;
    } else {
      res = nvec_.size();
      nvec_.resize( 1 + res );
    }
    idx_t i = hfn_( k )%hsize_;
    idx_t n = htab_[i];
    htab_[i] = res;
    node& nd = nvec_[res];
    nd.nxt_ = n;
    nd.key_ = k;
    ++nval_;
    return &htab_[i];
  }

  template<class T>
  typename T::val_t &hash_map<T>::ref( iter_t i )
  {
    return nvec_[*i].val_;
  }

  template<class T>
  typename T::val_t hash_map<T>::obj( iter_t i )
  {
    return nvec_[*i].val_;
  }

  template<class T>
  size_t hash_map<T>::size() const
  {
    return nval_;
  }

  template<class T>
  void hash_map<T>::clear()
  {
    nvec_.resize( 1 );
    rhd_  = 0;
    nval_ = 0;
    __builtin_memset( htab_, 0 , sizeof( htab_ ) );
  }

}
