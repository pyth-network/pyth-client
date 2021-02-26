#pragma once

namespace pc
{

  // node for doubly-linked list
  template<class T>
  class prev_next
  {
  public:
    prev_next();

    void set_next( T * );
    T *get_next() const;

    void set_prev( T * );
    T *get_prev() const;

  private:
    T *prev_;
    T *next_;
  };

  template<class T>
  prev_next<T>::prev_next()
  : prev_( nullptr ),
    next_( nullptr )
  {
  }

  template<class T>
  void prev_next<T>::set_next( T *next )
  {
    next_ = next;
  }

  template<class T>
  T *prev_next<T>::get_next() const
  {
    return next_;
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
