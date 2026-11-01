#include "types.h"

void *memset(void *dst, int c, uint n) {
  typedef uint64 __attribute__((__may_alias__)) word_t;
  uchar *p = dst;
  uchar byte = (uchar)c;
  uint64 word = (uint64)byte * 0x0101010101010101UL;

  while (n > 0 && ((uint64)p & 7) != 0) {
    *p++ = byte;
    n--;
  }
  while (n >= sizeof(word_t)) {
    *(word_t *)p = word;
    p += sizeof(word_t);
    n -= sizeof(word_t);
  }
  while (n > 0) {
    *p++ = byte;
    n--;
  }
  return dst;
}

int memcmp(const void *v1, const void *v2, uint n) {
  const uchar *s1, *s2;

  s1 = v1;
  s2 = v2;
  while (n-- > 0) {
    if (*s1 != *s2) return *s1 - *s2;
    s1++, s2++;
  }

  return 0;
}

void *memmove(void *dst, const void *src, uint n) {
  const char *s;
  char *d;

  s = src;
  d = dst;
  if (s < d && s + n > d) {
    s += n;
    d += n;
    while (n-- > 0) *--d = *--s;
  } else
    while (n-- > 0) *d++ = *s++;

  return dst;
}

// memcpy exists to placate GCC.  Use memmove.
void *memcpy(void *dst, const void *src, uint n) { return memmove(dst, src, n); }

int strncmp(const char *p, const char *q, uint n) {
  while (n > 0 && *p && *p == *q) n--, p++, q++;
  if (n == 0) return 0;
  return (uchar)*p - (uchar)*q;
}

char *strncpy(char *s, const char *t, int n) {
  char *os;

  os = s;
  while (n-- > 0 && (*s++ = *t++) != 0)
    ;
  while (n-- > 0) *s++ = 0;
  return os;
}

// Like strncpy but guaranteed to NUL-terminate.
char *safestrcpy(char *s, const char *t, int n) {
  char *os;

  os = s;
  if (n <= 0) return os;
  while (--n > 0 && (*s++ = *t++) != 0)
    ;
  *s = 0;
  return os;
}

int strlen(const char *s) {
  int n;

  for (n = 0; s[n]; n++)
    ;
  return n;
}
