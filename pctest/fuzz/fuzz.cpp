#include <stdint.h>
#include <oracle/oracle.h>
#include <oracle/pd.h>
#include <pc/manager.hpp>
#include <pc/log.hpp>
#include <unistd.h>
#include <signal.h>
#include <iostream>

using namespace pc;

void read_pd_t(pd_t* num) {
  std::cin >> num->v_;
  std::cin >> num->e_;
}

void assert(bool condition, std::string msg) {
  if (!condition) {
    std::cerr << msg << std::endl;
    abort();
  }
}

void test_pd_add(pd_t* num1, pd_t* num2) {
  std::cerr << "num1: " << num1->v_ << " * 10^" << num1->e_ << std::endl;
  std::cerr << "num2: " << num2->v_ << " * 10^" << num2->e_ << std::endl;

  // TODO: this table of numbers is repeated in a bunch of locations.
  const int64_t dec_fact[] = {
    1L, 10L, 100L, 1000L, 10000L, 100000L, 1000000L, 10000000L,
    100000000L, 1000000000L, 10000000000L, 100000000000L, 1000000000000L,
    10000000000000L, 100000000000000L, 1000000000000000L, 10000000000000000L,
    100000000000000000L
  };

  pd_t result;
  pd_add(&result, num1, num2, dec_fact);

  std::cerr << "result: " << result.v_ << " * 10^" << result.e_ << std::endl;

  // The sum of two positive numbers should be positive, and the
  // sum of two negative numbers should be negative.
  // TODO: test more interesting properties.
  assert(!(num1->v_ > 0 && num2->v_ > 0) || result.v_ >= 0, "sum of two positive numbers was not positive");
  assert(!(num1->v_ < 0 && num2->v_ < 0) || result.v_ <= 0, "sum of two negative numbers was not negative");
}

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
  if (argc < 2) {
    std::cerr << "Expected one argument (the function name to test)";
    return 1;
  }

  if (strcmp(argv[1], "add") == 0) {
    pd_t num1, num2;
    read_pd_t(&num1);
    read_pd_t(&num2);
    test_pd_add(&num1, &num2);
  } else {
    std::cerr << "Unknown test case: " << argv[1] << std::endl;
    return 1;
  }

  return 0;
}
