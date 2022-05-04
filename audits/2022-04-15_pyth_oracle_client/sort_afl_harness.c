#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#define static_assert(...) // ???

#include "../../oracle.h"

#define SORT_NAME  int64_sort_ascending
#define SORT_KEY_T int64_t
#include "sort_stable.c"

int cmpfunc (const void * a, const void * b) {
  return (int) ( *(int64_t*)a - *(int64_t*)b );
}

int main()
{
  // read testcase data
  uint8_t rawdata[128];
  memset(rawdata, 0, sizeof(rawdata));
  (void)!read(0, rawdata, sizeof(rawdata));

  // prepare testcase
  const size_t arr_size = PC_COMP_SIZE * 3;
  uint32_t nprcs;
  int64_t prcs[ arr_size ];
  int64_t scratch[ arr_size ];

  nprcs = 1 + ((size_t)rawdata[0] % arr_size);
  for (uint32_t i = 0; i < nprcs; i++) {
    prcs[i] = (int64_t)(int8_t)rawdata[1 + i];
  }

  // calculate correct result
  int64_t correct_result[ arr_size ];
  memcpy(correct_result, prcs, sizeof(prcs[0])*nprcs);
  qsort(correct_result, (size_t)nprcs, sizeof(prcs[0]), cmpfunc);

  // run testcase
  int64_t * sort_quote = int64_sort_ascending_stable( prcs, nprcs, scratch );

  // validate results
  for (uint32_t i = 0; i < nprcs; i++) {
    if (sort_quote[i] != correct_result[i]) {
      printf("Input validation failed!\n");
      printf("\nMismatch at index %d\n", i);
      printf("Sort result: ");
      for (uint32_t j = 0; j < nprcs; j++) {
        printf("%ld ", sort_quote[j]);
      }
      printf("\nCorrect result: ");
      for (uint32_t j = 0; j < nprcs; j++) {
        printf("%ld ", correct_result[j]);
      }
      abort();
    }
  }

  puts("OK");

  return 0;
}