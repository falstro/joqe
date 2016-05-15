#include "utf.h"

/* The ubiquitous UTF-8, variable width 1-4 bytes (1-6 bytes in the
 * original proposal). The number of leading 1s in the first byte gives
 * us the number of bytes. All continuation bytes have 10 in the upper
 * two bits.
 */
int
joqe_utf8(int a, int z, uint32_t *r)
{
  if(z) {
    if((a&0xc0) != 0x80) {
      // in continuation, but no continuation marker set.
      return -2;
    }
    *r = (*r << 6) | (a & 0x3F);
    return --z;
  } else if((a&0xc0) == 0xc0) {
    for(int x = a; x & 0x40; ++z, x <<= 1)
      ;
    if(z > 5) { // standard says max 3 (21 bits), original said 5 (31 bits).
      return -1;
    }
    *r = a & (0x3f >> z);
    return z;
  } else if (a&0x80) {
    // high bit set but not both, and we're not in continuation.
    return -1;
  } else {
    // single byte/7 bit ascii
    *r = a & 0xff;
    return 0;
  }
}

/* UTF-16, use variable width 2 or 4 bytes, 0xdc00 and 0xd800 mark high
 * and low surrogates respectively */
int
joqe_utf16(int a, int b, int z, uint32_t *r)
{
  uint16_t cu = (((a&0xff)<<8) | (b&0xff));
  if((cu & 0xfc00) == 0xd800) {
    // high surrogate
    *r |= (cu & 0x3ffu) << 10;
    *r += 0x10000;
  } else if ((cu & 0xfc00u) == 0xdc00u) {
    // low surrogate
    *r |= (cu & 0x3ffu);
  } else if(z) {
    // unpaired surrogate
    return -1;
  } else {
    *r = cu;
    return 0;
  }
  return !z;
}

/* UTF-32 (or UCS-4) are fixed width 4 bytes, no markers no nothing. */
uint32_t
joqe_utf32(int a, int b, int c, int d)
{
  uint32_t x = (a&0xff);
  x = (x<<8) | (b&0xff);
  x = (x<<8) | (c&0xff);
  x = (x<<8) | (d&0xff);
  return x;
}
