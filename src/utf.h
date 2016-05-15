#ifndef __JOQE_UTF_H__
#define __JOQE_UTF_H__

#include <stdint.h>

int   joqe_utf8  (int       a,
                  int       z,
                  uint32_t *codepoint);
int   joqe_utf16 (int       a, int b,
                  int       z,
                  uint32_t *codepoint);
uint32_t joqe_utf32 (int a, int b, int c, int d);


#endif /* idempotent include guard */
