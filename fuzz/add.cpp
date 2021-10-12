#include <stdint.h>
#include <oracle/oracle.h>
#include <oracle/pd.h>
#include <pc/manager.hpp>
#include <pc/log.hpp>
#include <unistd.h>
#include <signal.h>
#include <iostream>

using namespace pc;

// Adds two decimal numbers and asserts that the result has the correct sign.
// Usage: ./add
// then input four numbers on stdin:
// 1 <- value of the first number
// 2 <- exponent of the first number
// 3 <- value of the second number
// 4 <- exponent of the second number.
//
// see the files in add/testcases for example inputs
int main(int argc, char **argv)
{
  pd_t num1, num2;

  std::cin >> num1.v_;
  std::cin >> num1.e_;

  std::cin >> num2.v_;
  std::cin >> num2.e_;

  std::cerr << "num1: " << num1.v_ << " * 10^" << num1.e_ << std::endl;
  std::cerr << "num2: " << num2.v_ << " * 10^" << num2.e_ << std::endl;

  // TODO: this table of numbers is repeated in a bunch of locations.
  const int64_t dec_fact[] = {
    1L, 10L, 100L, 1000L, 10000L, 100000L, 1000000L, 10000000L,
    100000000L, 1000000000L, 10000000000L, 100000000000L, 1000000000000L,
    10000000000000L, 100000000000000L, 1000000000000000L, 10000000000000000L,
    100000000000000000L
  };

  pd_t result;
  pd_add(&result, &num1, &num2, dec_fact);

  std::cerr << "result: " << result.v_ << " * 10^" << result.e_ << std::endl;

  // The sum of two positive numbers should be positive, and the
  // sum of two negative numbers should be negative.
  // TODO: test more interesting properties.
  if (
    (num1.v_ > 0 && num2.v_ > 0 && result.v_ <= 0) ||
    (num1.v_ < 0 && num2.v_ < 0 && result.v_ >= 0)
  ) {
    abort();
  }

  return 0;
}
