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

int64_t cmpfunc (const void * a, const void * b) {
  return (int64_t) ( *(int64_t*)a - *(int64_t*)b );
}

void do_fuzz(int64_t*, size_t);

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if((size&0x7) != 0)size -= size&0x7;
    do_fuzz((int64_t *)data, size/8);
    return 0;

}

void do_fuzz(int64_t *data, size_t size)
{
  int64_t scratch[size];
  int64_t prcs[size];

  memcpy(prcs, data, size*sizeof(int64_t));

  int64_t* sort_quote = int64_sort_ascending_stable(prcs, size, scratch);

  for (uint32_t i = 1; i < size; i++) {
    if (sort_quote[i-1] > sort_quote[i]) {
      printf("Input validation failed!\n");
      printf("Mismatch at index %d\n", i);
      printf("Sort result: ");
      for (uint32_t j = 0; j < size; j++) {
        printf("%ld ", sort_quote[j]);
      }
      printf("\n");
      abort();
    }
  }

}

