#pragma once

namespace pc
{

  // doubly-linked list implementing prev_next<T>
  template<class T>
  class dbl_list
  {
  public:
    dbl_list();
    void add( T * );
    void del( T * );
    T *first() const;
  private:
    T *hd_;
    T *tl_;
  };

  template<class T>
  dbl_list<T>::dbl_list()
  : hd_( nullptr ),
    tl_( nullptr )
  {
  }

  template<class T>
  void dbl_list<T>::add( T *n )
  {
    n->set_prev( tl_ );
    n->set_next( nullptr );
    if ( tl_ ) {
      tl_->set_next( n );
    } else {
      hd_ = n;
    }
    tl_ = n;
  }

  template<class T>
  void dbl_list<T>::del( T *t )
  {
    T *p = t->get_prev();
    T *n = t->get_next();
    if ( p ) {
      p->set_next( n );
    } else {
      hd_ = n;
    }
    if ( n ) {
      n->set_prev( p );
    } else {
      tl_ = p;
    }
  }

  template<class T>
  T *dbl_list<T>::first() const
  {
    return hd_;
  }

}
