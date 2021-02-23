#pragma once

#include <exception>
#include <string>
#include <iostream>
#include <unistd.h>

#define PC_TEST_START  try{

#define PC_TEST_END } catch( lp::test_error& err ) {\
    std::cerr << err.what() << std::endl;\
    _exit( 1 );\
  } catch ( std::exception& err ) {\
    std::cerr << "exception err:" << err.what() << std::endl;\
    _exit( 1 );\
  }

#define PC_TEST_CHECK(X) \
if ( !(X) ) { throw lp::test_error( __FILE__, __LINE__); }

namespace lp
{
  class test_error : public std::exception
  {
  public:
    test_error( const std::string& filenm, int line )
    : val_( "test_error: file:" + filenm +
                 ", line:" + std::to_string( line ) ) {
    }
    const char *what() const noexcept {
      return val_.c_str();
    }
  private:
    std::string val_;
  };

}
