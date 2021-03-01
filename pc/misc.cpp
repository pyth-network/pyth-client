#include "misc.hpp"
#include <ctype.h>
#include <time.h>

namespace pc
{

const char * const ALPHABET =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
const char ALPHABET_MAP[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8, -1, -1, -1, -1, -1, -1,
    -1,  9, 10, 11, 12, 13, 14, 15, 16, -1, 17, 18, 19, 20, 21, -1,
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, -1, -1, -1, -1, -1,
    -1, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, -1, 44, 45, 46,
    47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

const double iFactor = 1.36565823730976103695740418120764243208481439700722980119458355862779176747360903943915516885072037696111192757109;

// reslen is the allocated length for result, feel free to overallocate
int enc_base58(const uint8_t *source, int len, uint8_t result[], int reslen) {
  int zeros = 0, length = 0, pbegin = 0, pend;
  if (!(pend = len)) return 0;
  while (pbegin != pend && !source[pbegin]) pbegin = ++zeros;
  int size = 1 + iFactor * (double)(pend - pbegin);
  uint8_t b58[size];
  for (int i = 0; i < size; i++) b58[i] = 0;
  while (pbegin != pend) {
    uint32_t carry = source[pbegin];
    int i = 0;
    for (int it1 = size - 1; (carry || i < length) && (it1 != -1); it1--,i++) {
      carry += 256 * b58[it1];
      b58[it1] = carry % 58;
      carry /= 58;
    }
    if (carry) return 0;
    length = i;
    pbegin++;
  }
  int it2 = size - length;
  while ((it2-size) && !b58[it2]) it2++;
  if ((zeros + size - it2 + 1) > reslen) return 0;
  int ri = 0;
  while(ri < zeros) result[ri++] = '1';
  for (; it2 < size; ++it2) result[ri++] = ALPHABET[b58[it2]];
  result[ri] = 0;
  return ri;
}

// result must be declared (for the worst case): char result[len * 2];
int dec_base58( const uint8_t *str, int len, uint8_t *result)
{
  result[0] = 0;
  int resultlen = 1;
  for (int i = 0; i < len; i++) {
    uint32_t carry = (uint32_t) ALPHABET_MAP[str[i]];
    if (carry == (uint32_t)-1) {
      break;
    }
    for (int j = 0; j < resultlen; j++) {
      carry += (uint32_t) (result[j]) * 58;
      result[j] = (uint8_t) (carry & 0xff);
      carry >>= 8;
    }
    while (carry > 0) {
      result[resultlen++] = carry & 0xff;
      carry >>= 8;
    }
  }

  for (int i = 0; i < len && str[i] == '1'; i++)
    result[resultlen++] = 0;

  // Poorly coded, but guaranteed to work.
  for (int i = resultlen - 1, z = (resultlen >> 1) + (resultlen & 1);
    i >= z; i--) {
    int k = result[i];
    result[i] = result[resultlen - i - 1];
    result[resultlen - i - 1] = k;
  }
  return resultlen;
}

char *uint_to_str( uint64_t val, char *cptr )
{
  if ( val ) {
    while( val ) {
      *--cptr = '0' + (val%10L);
      val /= 10L;
    }
  } else {
    *--cptr = '0';
  }
  return cptr;
}

uint64_t str_to_uint( const char *val, int len )
{
  uint64_t res = 0L;
  if ( len ) {
    const char *cptr = val;
    const char *end = &val[len];
    for(; cptr != end; ++cptr ) {
      if ( isdigit( *cptr ) ) {
        res = res*10UL + (*cptr-'0');
      } else {
        res = 0L;
        break;
      }
    }
  }
  return res;
}

char *int_to_str( int64_t val, char *cptr )
{
  if ( val ) {
    if ( val < 0 ) {
      *--cptr = '-';
      val = -val;
    }
    while( val ) {
      *--cptr = '0' + (val%10L);
      val /= 10L;
    }
  } else {
    *--cptr = '0';
  }
  return cptr;
}

int64_t str_to_int( const char *val, int len )
{
  bool is_neg = false;
  int64_t res = 0L;
  if ( len ) {
    const char *cptr = val;
    const char *end = &val[len];
    for(; cptr != end; ++cptr ) {
      if ( isdigit( *cptr ) ) {
        res = res*10UL + (*cptr-'0');
      } else if ( *cptr == '-' ) {
        is_neg = true;
      } else {
        res = 0L;
        break;
      }
    }
  }
  return is_neg ? -res : res;
}

//////////////////////////////////////////////////////////////////
// base64 encode/decode (with some formatting changes) courtesy of
// Adam Rudd per licence: github.com/adamvr/arduino-base64

static const char b64_alphabet[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789+/";

inline void a3_to_a4(uint8_t* a4, uint8_t* a3)
{
  a4[0] = (a3[0] & 0xfc) >> 2;
  a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
  a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6);
  a4[3] = (a3[2] & 0x3f);
}

inline void a4_to_a3( uint8_t* a3, uint8_t* a4)
{
  a3[0] = (a4[0] << 2) + ((a4[1] & 0x30) >> 4);
  a3[1] = ((a4[1] & 0xf) << 4) + ((a4[2] & 0x3c) >> 2);
  a3[2] = ((a4[2] & 0x3) << 6) + a4[3];
}

inline uint8_t b64_lookup(char c)
{
  if(c >='A' && c <='Z') return c - 'A';
  if(c >='a' && c <='z') return c - 71;
  if(c >='0' && c <='9') return c + 4;
  if(c == '+') return 62;
  if(c == '/') return 63;
  return -1;
}

int enc_base64_len( int n )
{
  return (n + 2 - ((n + 2) % 3)) / 3 * 4;
}

int enc_base64( const uint8_t *inp, int len, uint8_t *out )
{
  int i = 0, j = 0, encLen = 0;
  uint8_t a3[3];
  uint8_t a4[4];

  while( len-- ) {
    a3[i++] = *(inp++);
    if(i == 3) {
      a3_to_a4(a4, a3);
      for(i = 0; i < 4; i++) {
        out[encLen++] = b64_alphabet[a4[i]];
      }
      i = 0;
    }
  }

  if(i) {
    for(j = i; j < 3; j++) {
      a3[j] = '\0';
    }
    a3_to_a4(a4, a3);
    for(j = 0; j < i + 1; j++) {
      out[encLen++] = b64_alphabet[a4[j]];
    }

    while((i++ < 3)) {
      out[encLen++] = '=';
    }
  }
  return encLen;
}

int dec_base64( const uint8_t *inp, int len, uint8_t *out )
{
  int i = 0, j = 0, decLen = 0;
  uint8_t a3[3];
  uint8_t a4[4];

  while ( len-- ) {
    if(*inp == '=') {
      break;
    }
    a4[i++] = *(inp++);
    if (i == 4) {
      for (i = 0; i <4; i++) {
        a4[i] = b64_lookup(a4[i]);
      }
      a4_to_a3(a3,a4);
      for (i = 0; i < 3; i++) {
        out[decLen++] = a3[i];
      }
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 4; j++) {
      a4[j] = '\0';
    }
    for (j = 0; j <4; j++) {
      a4[j] = b64_lookup(a4[j]);
    }
    a4_to_a3(a3,a4);
    for (j = 0; j < i - 1; j++) {
      out[decLen++] = a3[j];
    }
  }
  return decLen;
}

int64_t get_now()
{
  struct timespec ts[1];
  clock_gettime( CLOCK_REALTIME, ts );
  int64_t res = ts->tv_sec;
  res *= 1000000000UL;
  res += ts->tv_nsec;
  return res;
}

void uint_to_str6( char *cptr, int64_t val )
{
  cptr[5] = '0' + (val%10L); val/=10L;
  cptr[4] = '0' + (val%10L); val/=10L;
  cptr[3] = '0' + (val%10L); val/=10L;
  cptr[2] = '0' + (val%10L); val/=10L;
  cptr[1] = '0' + (val%10L); val/=10L;
  cptr[0] = '0' + (val%10L);
}

void uint_to_str4( char *cptr, int64_t val )
{
  cptr[3] = '0' + (val%10L); val/=10L;
  cptr[2] = '0' + (val%10L); val/=10L;
  cptr[1] = '0' + (val%10L); val/=10L;
  cptr[0] = '0' + (val%10L);
}

void uint_to_str2( char *cptr, int64_t val )
{
  cptr[1] = '0' + (val%10L); val/=10L;
  cptr[0] = '0' + (val%10L);
}

char *nsecs_to_utc6( int64_t ts, char *cptr )
{
  int64_t nsecs = ts%PC_NSECS_IN_SEC;
  ts /= PC_NSECS_IN_SEC;
  struct tm t[1];
  gmtime_r( &ts, t );
  uint_to_str4( &cptr[0], t->tm_year + 1900 );
  uint_to_str2( &cptr[5], t->tm_mon + 1 );
  uint_to_str2( &cptr[8], t->tm_mday );
  uint_to_str2( &cptr[11], t->tm_hour );
  uint_to_str2( &cptr[14], t->tm_min );
  uint_to_str2( &cptr[17], t->tm_sec );
  uint_to_str6( &cptr[20], nsecs/1000L );
  cptr[4] = cptr[7] = '-';
  cptr[10] = 'T';
  cptr[13] = cptr[16] = ':';
  cptr[19] = '.';
  cptr[26] = 'Z';
  cptr[27] = '\0';
  return cptr;
}

}
