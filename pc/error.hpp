#pragma once

#include <string>
#include <string.h>

namespace pc
{

  // container for general errors
  class error
  {
  public:
    error();
    void reset_err();
    bool get_is_err() const;
    bool set_err_msg( const std::string& );
    bool set_err_msg( const std::string&, int errcode );
    std::string get_err_msg() const;
  private:
    bool        is_err_;
    std::string err_msg_;
  };

  inline error::error()
  : is_err_( false )
  {
  }

  inline void error::reset_err()
  {
    is_err_ = false;
    err_msg_.clear();
  }

  inline bool error::get_is_err() const
  {
    return is_err_;
  }

  inline bool error::set_err_msg( const std::string& err_msg )
  {
    err_msg_ = err_msg;
    is_err_ = true;
    return false;
  }

  inline bool error::set_err_msg( const std::string& err_msg, int errcode )
  {
    err_msg_ = err_msg;
    err_msg_ += " [";
    err_msg_ += std::to_string( errcode );
    err_msg_ += ' ';
    err_msg_ += strerror( errcode);
    err_msg_ += ']';
    is_err_ = true;
    return false;
  }

  inline std::string error::get_err_msg() const
  {
    return err_msg_;
  }


}
