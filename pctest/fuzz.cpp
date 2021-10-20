#include <stdint.h>
#include <oracle/oracle.h>
#include <oracle/pd.h>
#include <oracle/upd_aggregate.h>
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

// Read a pd_t from cin. Expects two numbers on successive lines of the file, e.g.
// 2
// 3
// is parsed as 2 * 10^3.
// Note that this scales the input number so that arithmetic operations can use the result.
void read_pd_t(pd_t* num) {
  int32_t e = 0;
  int64_t v = 0;

  std::cin >> v;
  std::cin >> e;

  pd_new_scale(num, v, e);
}

// Assert that condition is true. If condition is false, print msg and abort.
void assert(bool condition, std::string msg) {
  if (!condition) {
    std::cout << msg << std::endl;
    abort();
  }
}

// Print a pd_t to cout.
void log_pd_t(std::string prefix, pd_t* num) {
  std::cout << prefix << num->v_ << " * 10^" << num->e_ << std::endl;
}

// Return true if 10^e can be represented in a double.
bool exponent_in_double_range(int e) {
  // double represents 2^-1024 - 2^1024, which is roughly 10^300.
  return -300 < e && e < 300;
}

// Convert a pd_t to a double, where the exponent is num->e - offset.
// (Use offset to map arithmetic operations into a range that double can represent.)
double pd_to_double(pd_t* num, int offset) {
  return num->v_ * pow(10, num->e_ - offset);
}

void test_pd_add(pd_t* num1, pd_t* num2) {
  log_pd_t("num1: ", num1);
  log_pd_t("num2: ", num2);

  pd_t result[1];
  pd_add(result, num1, num2, dec_fact);

  log_pd_t("result: ", result);

  int exponent_delta = num1->e_ - num2->e_;
  if (!exponent_in_double_range(exponent_delta)) {
    pd_t* max_num;
    if (num1->e_ > num2->e_) {
      max_num = num1;
    } else {
      max_num = num2;
    }

    // Adding two numbers that differ by a factor of > 10^600 should give you the larger number.
    log_pd_t("expected: ", max_num);
    log_pd_t("actual: ", result);
    assert(result->v_ == max_num->v_ && result->e_ == max_num->e_, "Result was not equal to the larger of the two numbers");
  } else {
    int min_e = std::min(num1->e_, num2->e_);
    int max_e = std::max(num1->e_, num2->e_);

    double num1_f = pd_to_double(num1, min_e);
    double num2_f = pd_to_double(num2, min_e);
    double expected = num1_f + num2_f;
    double actual = pd_to_double(result, min_e);

    // Adding two numbers can increase the exponent by at most 1.
    // (The exact exponent is somewhat unpredictable due to how the code normalizes numbers.)
    assert(result->e_ <= 1 + max_e, "Result exponent should be at most 1 larger than largest input exponent.");

    double diff = fabs(expected - actual);
    // The error introduced by addition is entirely due to truncating the number at the result exponent.
    double max_diff = pow(10, result->e_ - min_e);
    std::cout << "expected: " << expected << " actual: " << actual << std::endl;
    std::cout << "diff: " << diff << " max diff: " << max_diff << std::endl;
    assert(diff <= max_diff, "difference between fixed point and floating point was too large");
  }
}

void test_pd_sub(pd_t* num1, pd_t* num2) {
  log_pd_t("num1: ", num1);
  log_pd_t("num2: ", num2);

  pd_t result[1];
  pd_sub(result, num1, num2, dec_fact);

  log_pd_t("result: ", result);

  int exponent_delta = num1->e_ - num2->e_;
  if (!exponent_in_double_range(exponent_delta)) {
    pd_t expected[1];
    if (num1->e_ > num2->e_) {
      expected->v_ = num1->v_;
      expected->e_ = num1->e_;
    } else {
      expected->v_ = -1 * num2->v_;
      expected->e_ = num2->e_;
    }

    log_pd_t("expected: ", expected);
    log_pd_t("actual: ", result);
    assert(result->v_ == expected->v_ && result->e_ == expected->e_, "Result was not equal to the number with the larger exponent");
  } else {
    int min_e = std::min(num1->e_, num2->e_);
    int max_e = std::max(num1->e_, num2->e_);

    double num1_f = pd_to_double(num1, min_e);
    double num2_f = pd_to_double(num2, min_e);

    double expected = num1_f - num2_f;
    double actual = pd_to_double(result, min_e);

    // Subtracting two numbers can increase the exponent by at most 1.
    // (The exact exponent is somewhat unpredictable due to how the code normalizes numbers.)
    assert(result->e_ <= 1 + max_e, "Result exponent should be at most 1 larger than largest input exponent.");

    double diff = fabs(expected - actual);
    // The error introduced by addition is entirely due to truncating the number at the result exponent.
    double max_diff = pow(10, result->e_ - min_e);
    std::cout << "expected: " << expected << " actual: " << actual << std::endl;
    std::cout << "diff: " << diff << " max diff: " << max_diff << std::endl;
    assert(diff <= max_diff, "difference between fixed point and floating point was too large");
  }
}

void test_pd_mul(pd_t* num1, pd_t* num2) {
  log_pd_t("num1: ", num1);
  log_pd_t("num2: ", num2);

  pd_t result[1];
  pd_mul(result, num1, num2);

  log_pd_t("result: ", result);

  int exponent_delta = num1->e_ - num2->e_;
  if (!exponent_in_double_range(exponent_delta)) {
    // It's hard to test this case with double.
    std::cout << "range too large, skipping test" << std::endl;
  } else {
    int min_e = std::min(num1->e_, num2->e_);
    int max_e = std::max(num1->e_, num2->e_);

    double num1_f = pd_to_double(num1, min_e);
    double num2_f = pd_to_double(num2, min_e);

    double expected = num1_f * num2_f;
    double actual = pd_to_double(result, (2 * min_e));

    double diff = fabs(expected - actual);
    double max_diff = pow(10, result->e_ - (2 * min_e));
    std::cout << "expected: " << expected << " actual: " << actual << std::endl;
    std::cout << "diff: " << diff << " max diff: " << max_diff << std::endl;
    assert(max_e + min_e - 10 <= result->e_ && result->e_ <= max_e + min_e + 1, "Result exponent is not in expected range");
    assert(diff <= max_diff, "difference between fixed point and floating point was too large");
  }
}

void test_pd_div(pd_t* num1, pd_t* num2) {
  log_pd_t("num1: ", num1);
  log_pd_t("num2: ", num2);

  if (num2->v_ == 0) {
    std::cout << "Skipping test: cannot divide by 0." << std::endl;
    return;
  }

  pd_t result[1];
  pd_div(result, num1, num2);

  log_pd_t("result: ", result);

  int exponent_delta = num1->e_ - num2->e_;
  if (!exponent_in_double_range(exponent_delta)) {
    std::cout << "range too large, skipping test" << std::endl;
  } else {
    int min_e = std::min(num1->e_, num2->e_);

    double num1_f = pd_to_double(num1, min_e);
    double num2_f = pd_to_double(num2, min_e);

    double expected = num1_f / num2_f;
    double actual = pd_to_double(result, 0);

    double diff = fabs(expected - actual);
    double max_diff = pow(10, result->e_);

    std::cout << "expected: " << expected << " actual: " << actual << std::endl;
    std::cout << "diff: " << diff << " max diff: " << max_diff << std::endl;

    assert(num1->e_ - (num2->e_ + 10) <= result->e_ && result->e_ <= num1->e_ + 1 - num2->e_, "Result exponent not in expected range");
    // zero can be stored with multiple exponents, and the exponent may be larger than the range representable by double
    // because it doesn't get normalized.
    assert((num1->v_ == 0 && result->v_ == 0) || diff <= max_diff, "difference between fixed point and floating point was too large");
  }
}

void test_upd_ema(int64_t n, int64_t d, pd_t* val) {
  pd_t numer[1], denom[1];
  pd_load(numer, n);
  pd_load(denom, d);

  log_pd_t("numer: ", numer);
  log_pd_t("denom: ", denom);
  log_pd_t("val: ", val);

  if (numer->v_ < 0 || denom->v_ <= 0) {
    std::cout << "Skipping test: numerator must be >= 0 and denominator must be > 0." << std::endl;
    return;
  }

  if (exponent_in_double_range(val->e_)) {
    // There are only 5 bits of exponent in the int64_t representation, so numer/denom definitely fit in a double.
    double numer_f = pd_to_double(numer, 0);
    double denom_f = pd_to_double(denom, 0);
    double val_f = pd_to_double(val, 0);
    pd_t decay[1];
    pd_new(decay, PD_EMA_DECAY, PD_EMA_EXPO);
    double decay_f = pd_to_double(decay, 0);

    std::cout << "numer_f: " << numer_f << std::endl;
    std::cout << "denom_f: " << denom_f << std::endl;
    std::cout << "val_f: " << val_f << std::endl;
    std::cout << "decay_f: " << decay_f << std::endl;

    double expected = (numer_f * (1 + decay_f) + val_f) / (denom_f * (1 + decay_f) + 1);

    // Build inputs to EMA and run it.
    pc_ema_t ema[1];
    ema->numer_ = n;
    ema->denom_ = d;

    // TODO: make this configurable
    pd_t conf[1];
    conf->v_ = 1;
    conf->e_ = 0;

    pc_qset_t qs[1];
    for (int i = 0; i < 18; i++) {
      qs->fact_[i] = dec_fact[i];
    }
    qs->expo_ = -9;

    pc_price_t prc[1];
    prc->drv1_ = 1;
    upd_ema(ema, val, conf, 1, qs, prc);

    pd_t result[1];
    result->v_ = ema->val_;
    result->e_ = qs->expo_;

    log_pd_t("result: ", result);
    double actual = pd_to_double(result, 0);

    // Check the diff two ways. If the diff is small, then the truncation at the exponent result->e_ will be
    // the more significant factor. If the diff is large, then check that it's close in relative terms.
    double diff = fabs(expected - actual);
    double max_diff = pow(10, result->e_);

    double rel_diff = diff / expected;
    double max_rel_diff = pow(10, -7);

    std::cout << "expected: " << expected << " actual: " << actual << std::endl;
    std::cout << "diff: " << diff << " max diff: " << max_diff << std::endl;
    std::cout << "relative diff: " << rel_diff << " max relative diff: " << max_rel_diff << std::endl;
    assert((expected == 0 && actual == 0) || diff <= max_diff || rel_diff <= max_rel_diff, "difference between fixed point and floating point was too large");
  }
}

void test_pd_adjust(pd_t* val, int e) {
  if (exponent_in_double_range(val->e_) && exponent_in_double_range(e)) {
    log_pd_t("initial: ", val);
    double initial_value = pd_to_double(val, 0);
    pd_adjust(val, e, dec_fact);
    log_pd_t("result: ", val);

    double final_value = pd_to_double(val, 0);
    double diff = fabs(final_value - initial_value);
    double max_diff = pow(10, val->e_);

    std::cout << "initial: " << initial_value << " final: " << final_value << std::endl;
    std::cout << "diff: " << diff << " max diff: " << max_diff << std::endl;
    assert(diff <= max_diff, "difference between fixed point and floating point was too large");
  }
}

// Runs a fuzz test.
// Usage: ./fuzz <test name>
// then provide test input on stdin.
int main(int argc, char **argv)
{
  if (argc < 2) {
    std::cout << "Expected one argument (the function name to test)";
    return 1;
  }

  std::cout.precision(12);

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
    int64_t numer, denom;
    pd_t val;
    std::cin >> numer;
    std::cin >> denom;
    read_pd_t(&val);
    test_upd_ema(numer, denom, &val);
  } else if (strcmp(argv[1], "adjust") == 0) {
    int e;
    pd_t val;
    read_pd_t(&val);
    std::cin >> e;
    test_pd_adjust(&val, e);
  } else {
    std::cout << "Unknown test case: " << argv[1] << std::endl;
    return 1;
  }

  return 0;
}
