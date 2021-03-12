#pragma once

namespace pc
{

  // doubly-linked list implementing prev_next<T>
  template<class T>
  class dbl_list
  {
  public:
    dbl_list();
    void clear();
    bool empty() const;
    void add( T * );
    void del( T * );
    T *first() const;
  private:
    T *hd_;
    T *tl_;
  };

  // node for single-linked list
  template<class T>
  class next
  {
  public:
    next();

    void set_next( T * );
    T *get_next() const;

  private:
    T *next_;
  };

  // node for doubly-linked list
  template<class T>
  class prev_next : public next<T>
  {
  public:
    prev_next();

    void set_prev( T * );
    T *get_prev() const;

  private:
    T *prev_;
  };


  template<class T>
  dbl_list<T>::dbl_list()
  : hd_( nullptr ),
    tl_( nullptr )
  {
  }

  template<class T>
  void dbl_list<T>::clear()
  {
    hd_ = tl_ = nullptr;
  }

  template<class T>
  bool dbl_list<T>::empty() const
  {
    return hd_ == nullptr;
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

  template<class T>
  next<T>::next()
  : next_( nullptr )
  {
  }

  template<class T>
  void next<T>::set_next( T *next )
  {
    next_ = next;
  }

  template<class T>
  T *next<T>::get_next() const
  {
    return next_;
  }

  template<class T>
  prev_next<T>::prev_next()
  : prev_( nullptr )
  {
  }

  template<class T>
  void prev_next<T>::set_prev( T *prev )
  {
    prev_= prev;
  }

  template<class T>
  T *prev_next<T>::get_prev() const
  {
    return prev_;
  }


}
