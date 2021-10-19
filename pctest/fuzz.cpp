#include <stdint.h>
#include <oracle/oracle.h>
#include <oracle/pd.h>
#include <pc/manager.hpp>
#include <pc/log.hpp>
#include <unistd.h>
#include <signal.h>
#include <iostream>
#include <math.h>

using namespace pc;

// TODO: this table of numbers is repeated in a bunch of locations.
const int64_t dec_fact[] = {
  1L, 10L, 100L, 1000L, 10000L, 100000L, 1000000L, 10000000L,
  100000000L, 1000000000L, 10000000000L, 100000000000L, 1000000000000L,
  10000000000000L, 100000000000000L, 1000000000000000L, 10000000000000000L,
  100000000000000000L
};

void read_pd_t(pd_t* num) {
  int32_t e;
  int64_t v;

  std::cin >> v;
  std::cin >> e;

  pd_new_scale(num, v, e);
}

void assert(bool condition, std::string msg) {
  if (!condition) {
    std::cerr << msg << std::endl;
    abort();
  }
}

void log_pd_t(std::string prefix, pd_t* num) {
  std::cerr << prefix << num->v_ << " * 10^" << num->e_ << std::endl;
}

void test_pd_add(pd_t* num1, pd_t* num2) {
  log_pd_t("num1: ", num1);
  log_pd_t("num2: ", num2);

  pd_t result;
  pd_add(&result, num1, num2, dec_fact);

  std::cerr << "result: " << result.v_ << " * 10^" << result.e_ << std::endl;

  int exponent_delta = num1->e_ - num2->e_;
  // max range of double is 2^1024, which is roughly 10^300
  if (fabs(exponent_delta) > 300) {
    // the sum of these numbers is not representable in double-precision floating point

    pd_t* max_num;
    if (num1->e_ > num2->e_) {
      max_num = num1;
    } else {
      max_num = num2;
    }

    std::cerr << "expected: " << max_num->v_ << " * 10^" << max_num->e_ << std::endl;
    assert(result.v_ == max_num->v_ && result.e_ == max_num->e_, "Result was not equal to the larger of the two numbers");
  } else {
    int min_e, max_e;
    if (num1->e_ > num2->e_) {
      min_e = num2->e_;
      max_e = num1->e_;
    } else {
      min_e = num1->e_;
      max_e = num2->e_;
    }

    double num1_f = num1->v_ * pow(10, num1->e_ - min_e);
    double num2_f = num2->v_ * pow(10, num2->e_ - min_e);

    double expected = num1_f + num2_f;
    double actual = result.v_ * pow(10, result.e_ - min_e);

    double diff = fabs(expected - actual);
    // maximum permissible error is the precision of the number with the larger exponent + carry.
    double max_diff = pow(10, result.e_ - min_e);

    // note that this assertion validates the computation of max_diff above.
    int carry = result.e_ - min_e;
    assert(0 <= carry && carry <= 1 + (max_e - min_e), "result exponent should be at most 1 larger than input.");

    std::cerr << "expected: " << expected << " actual: " << actual << std::endl;
    std::cerr << "diff: " << diff << " max diff: " << max_diff << std::endl;

    assert(diff <= max_diff, "difference between fixed point and floating point was too large");
  }
}

void test_pd_sub(pd_t* num1, pd_t* num2) {
  std::cerr << "num1: " << num1->v_ << " * 10^" << num1->e_ << std::endl;
  std::cerr << "num2: " << num2->v_ << " * 10^" << num2->e_ << std::endl;

  pd_t result;
  pd_sub(&result, num1, num2, dec_fact);

  std::cerr << "result: " << result.v_ << " * 10^" << result.e_ << std::endl;

  int exponent_delta = num1->e_ - num2->e_;
  // max range of double is 2^1024, which is roughly 10^300
  if (fabs(exponent_delta) > 300) {
    // the difference between these numbers is not representable in double-precision floating point
    pd_t expected;
    if (num1->e_ > num2->e_) {
      expected.v_ = num1->v_;
      expected.e_ = num1->e_;
    } else {
      expected.v_ = -1 * num2->v_;
      expected.e_ = num2->e_;
    }

    std::cerr << "expected: " << expected.v_ << " * 10^" << expected.e_ << std::endl;
    assert(result.v_ == expected.v_ && result.e_ == expected.e_, "Result was not equal to the number with the larger exponent");
  } else {
    int min_e, max_e;
    if (num1->e_ > num2->e_) {
      min_e = num2->e_;
      max_e = num1->e_;
    } else {
      min_e = num1->e_;
      max_e = num2->e_;
    }

    double num1_f = num1->v_ * pow(10, num1->e_ - min_e);
    double num2_f = num2->v_ * pow(10, num2->e_ - min_e);

    double expected = num1_f - num2_f;
    double actual = result.v_ * pow(10, result.e_ - min_e);

    double diff = fabs(expected - actual);
    // maximum permissible error is the precision of the number with the larger exponent + carry.
    double max_diff = pow(10, result.e_ - min_e);

    // note that this assertion validates the computation of max_diff above.
    int carry = result.e_ - min_e;
    assert(-1 <= carry && carry <= max_e - min_e, "result exponent should be at most 1 less than largest input exponent.");

    std::cerr << "expected: " << expected << " actual: " << actual << std::endl;
    std::cerr << "diff: " << diff << " max diff: " << max_diff << std::endl;

    assert(diff <= max_diff, "difference between fixed point and floating point was too large");
  }
}

void test_pd_mul(pd_t* num1, pd_t* num2) {
  std::cerr << "num1: " << num1->v_ << " * 10^" << num1->e_ << std::endl;
  std::cerr << "num2: " << num2->v_ << " * 10^" << num2->e_ << std::endl;

  pd_t result;
  pd_mul(&result, num1, num2);

  std::cerr << "result: " << result.v_ << " * 10^" << result.e_ << std::endl;

  int exponent_delta = num1->e_ - num2->e_;
  // max range of double is 2^1024, which is roughly 10^300
  if (fabs(exponent_delta) > 300) {
    std::cerr << "range too large, skipping test" << std::endl;
  } else {
    int min_e, max_e;
    if (num1->e_ > num2->e_) {
      min_e = num2->e_;
      max_e = num1->e_;
    } else {
      min_e = num1->e_;
      max_e = num2->e_;
    }

    double num1_f = num1->v_ * pow(10, num1->e_ - min_e);
    double num2_f = num2->v_ * pow(10, num2->e_ - min_e);

    double expected = num1_f * num2_f;
    double actual = result.v_ * pow(10, result.e_ - (2 * min_e));

    double diff = fabs(expected - actual);
    // maximum permissible error is the precision of the number with the larger exponent + carry.
    double max_diff = pow(10, result.e_ - (2 * min_e));

    // note that this assertion validates the computation of max_diff above.
    // int carry = result.e_ - min_e;
    // assert(-1 <= carry && carry <= max_e - min_e, "result exponent should be at most 1 less than largest input exponent.");

    std::cerr << "expected: " << expected << " actual: " << actual << std::endl;
    std::cerr << "diff: " << diff << " max diff: " << max_diff << std::endl;

    assert(diff <= max_diff, "difference between fixed point and floating point was too large");
  }
}

void test_pd_div(pd_t* num1, pd_t* num2) {
  std::cerr << "num1: " << num1->v_ << " * 10^" << num1->e_ << std::endl;
  std::cerr << "num2: " << num2->v_ << " * 10^" << num2->e_ << std::endl;

  if (num2->v_ == 0) {
    std::cerr << "Skipping test: cannot divide by 0." << std::endl;
    return;
  }

  pd_t result;
  pd_div(&result, num1, num2);

  std::cerr << "result: " << result.v_ << " * 10^" << result.e_ << std::endl;

  int exponent_delta = num1->e_ - num2->e_;
  // max range of double is 2^1024, which is roughly 10^300
  if (fabs(exponent_delta) > 300) {
    std::cerr << "range too large, skipping test" << std::endl;
  } else {
    int min_e, max_e;
    if (num1->e_ > num2->e_) {
      min_e = num2->e_;
      max_e = num1->e_;
    } else {
      min_e = num1->e_;
      max_e = num2->e_;
    }

    double num1_f = num1->v_ * pow(10, num1->e_ - min_e);
    double num2_f = num2->v_ * pow(10, num2->e_ - min_e);

    double expected = num1_f / num2_f;
    double actual = result.v_ * pow(10, result.e_);

    double diff = fabs(expected - actual);
    // maximum permissible error is the precision of the number with the larger exponent + carry.
    double max_diff = pow(10, result.e_);

    // note that this assertion validates the computation of max_diff above.
    // int carry = result.e_ - min_e;
    // assert(-1 <= carry && carry <= max_e - min_e, "result exponent should be at most 1 less than largest input exponent.");

    std::cerr << "expected: " << expected << " actual: " << actual << std::endl;
    std::cerr << "diff: " << diff << " max diff: " << max_diff << std::endl;

    // zero can be stored with multiple exponents, and the exponent may be larger than the range representable by double
    // because it doesn't get normalized.
    assert((num1->v == 0 && result->v == 0) || diff <= max_diff, "difference between fixed point and floating point was too large");
  }
}

void test_upd_ema(int64_t n, int64_t d, pd_t* val) {
  pd_t numer, denom;
  pd_load(&numer, n);
  pd_load(&denom, d);

  log_pd_t("numer: ", numer);
  log_pd_t("denom: ", denom);

  if (numer->v_ < 0 || denom->v_ <= 0) {
    std::cerr << "Skipping test: numerator must be >= 0 and denominator must be > 0." << std::endl;
    return;
  }

  pc_ema_t ema;
  ema.numer_ = n;
  ema.denom_ = d;

  // TODO:
  pd_t conf;
  conf.v_ = 1;
  conf.e_ = 1;

  pc_qset_t qs;
  qs.fact_ = dec_fact;
  qs.expo_ = -9;

  pc_price_t prc;
  prc.drv1_ = 1;

  upd_ema(&ema, val, conf, 1, &qs, &prc);

  pd_t result[1];
  result->v_ = ema.val_;
  result->e_ = qs.expo_;

  log_pd_t("result: ", result);

  // there are only 5 bits of exponent in the int64_t representation, so it definitely fits in a double.
  if (val->e < 300 && val-> > -300) {
    double numer_f = numer->v_ * pow(10, numer->e_);
    double denom_f = denom->v_ * pow(10, denom->e_);
    double val_f = val->v_ * pow(10, val->e_);

    double expected = (numer_f / denom_f) * decay + val_f;
    double actual = result.v_ * pow(10, result.e_);

    double diff = fabs(expected - actual);
    // maximum permissible error is the precision of the number with the larger exponent + carry.
    double max_diff = pow(10, result.e_);

    std::cerr << "expected: " << expected << " actual: " << actual << std::endl;
    std::cerr << "diff: " << diff << " max diff: " << max_diff << std::endl;

    assert(diff <= max_diff, "difference between fixed point and floating point was too large");
  }
}

// Runs a fuzz test.
// Usage: ./fuzz <test name>
// then provide test input on stdin.
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
  } else if (strcmp(argv[1], "sub") == 0) {
    pd_t num1, num2;
    read_pd_t(&num1);
    read_pd_t(&num2);
    test_pd_sub(&num1, &num2);
  } else if (strcmp(argv[1], "mul") == 0) {
    pd_t num1, num2;
    read_pd_t(&num1);
    read_pd_t(&num2);
    test_pd_mul(&num1, &num2);
  } else if (strcmp(argv[1], "div") == 0) {
    pd_t num1, num2;
    read_pd_t(&num1);
    read_pd_t(&num2);
    test_pd_div(&num1, &num2);
  } else if (strcmp(argv[1], "ema") == 0) {
    int64_t, numer, denom;
    pd_t val;
    std::cin >> numer;
    std::cin >> denom;
    read_pd_t(&val);
    test_upd_ema(numer, denom, &val);
  } else {
    std::cerr << "Unknown test case: " << argv[1] << std::endl;
    return 1;
  }

  return 0;
}
