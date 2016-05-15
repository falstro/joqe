#include "lex-source.h"
#include "err.h"
#include "utf.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <fcntl.h>

#include <sys/uio.h>

#include <assert.h>

#define BUFSZ   0x8000
#define BUFMASK (BUFSZ-1)

// #define UTF8_BYPASS
/* NOTE: If we bypass UTF8 parsing, string comparisons might fail on
   specifically crafted UTF8 strings using overlong encodings. For
   example, the ASCII space character could either be represented using
   a single byte 0x20, or using two bytes 0xc0 0xd0, or three 0xe0 0x80
   0xd0. This is not a valid UTF8 stream, but could potentially be
   exploited if joqe is used to verify data.
*/

#ifndef UTF_REPLACEMENT
#define UTF_REPLACEMENT 0xfffd
#endif
// to make sure the replacement character is ours, and not from the
// terminal, we can replace it with something else temporarily, like
// biohazard: 0x2623

static int
detect_byte_order(const char *s, int len, int *bom)
{
  const unsigned char *us = (const unsigned char*) s;
  int f = 0;
  *bom = 0;
  if(len > 1) {
    if(!us[0]) {
      if(us[1]) f = JOQE_LEX_UTF16;
      else if(len > 3) {
        f = JOQE_LEX_UTF32;
        if(us[2] == 254 && us[3] == 255) {
          *bom = 4;
        }
      }
    } else if(!us[1]) {
      f = JOQE_LEX_MB_LE;
      if(len > 3 && !us[2]) f |= JOQE_LEX_UTF32;
      else f |= JOQE_LEX_UTF16;
    } else if (us[0] == 255 && us[1] == 254) {
      //LE, 16 or 32
      f = JOQE_LEX_MB_LE;
      if(len > 3 && us[2] == 0 && us[3] == 0) {
        f |= JOQE_LEX_UTF32;
        *bom = 4;
      } else {
        f |= JOQE_LEX_UTF16;
        *bom = 2;
      }
    } else if (us[0] == 254 && us[1] == 255) {
      f = JOQE_LEX_UTF16;
      *bom = 2;
    } else if (len > 2 && us[0] == 0xef && us[1] == 0xbb && us[2] == 0xbf) {
      // UTF-8 BOM.
      *bom = 3;
    }
  }
  return f;
}

int
joqe_lex_source_push(joqe_lex_source *s, uint32_t cp)
{
  // render UTF-8
  if(cp < 0x80)
    return s->c = cp;

  assert(!s->mbi);

  // High bit should never be set, and is not possible to encode in UTF-8
  // (which is limited to 31 bits in theory and the original proposal, 21 bits
  // in the UTF-8 specification).
  if(cp & 0x80000000u)
    return s->c = -1;

  int mask = 0x3f;
  do {
    mask >>= 1;
    assert(mask);
    assert(s->mbi < 5);

    s->mb[s->mbi++] = (cp & 0x3f) | 0x80;
    cp >>= 6;
  } while(cp > mask);

  return s->c = ((cp & mask)|((~mask)<<1))&0xff;
}

int
joqe_lex_source_read(joqe_lex_source *s)
{
  if(s->mbi) // pushed utf-8 multi-bytes.
    return (s->c = s->mb[--s->mbi]);

  int f = s->f;
  uint32_t cp = 0;

  if(f&JOQE_LEX_UTF16) {
    int a, b, z = 0;
    do {
      a = s->read(s);
      b = s->read(s);
      if((a|b) < 0)
        return (s->c = -1);
      if(f & JOQE_LEX_MB_LE) { a ^= b; b ^= a; a ^= b; }
    } while((z = joqe_utf16(a, b, z, &cp)) > 0);

    if(z < 0) {
      /* invalid utf-16 */
      cp = UTF_REPLACEMENT;
    }
  } else if (f&JOQE_LEX_UTF32) {
    int a = s->read(s);
    int b = s->read(s);
    int c = s->read(s);
    int d = s->read(s);
    if((a|b|c|d) < 0) {
      return (s->c = -1);
    }
    if(f & JOQE_LEX_MB_LE) {
      cp = joqe_utf32(d,c,b,a);
    } else {
      cp = joqe_utf32(a,b,c,d);
    }
  } else {
#ifdef UTF8_BYPASS
    int a = s->read(s);
    if(a < 0)
      return (s->c = -1);
    return (s->c = a&0xff);
#else
    int a, z = 0;
    uint32_t masks[] = {0,0x7f,0x7ff,0xffff,0x1fffff,0x3ffffff,0x7fffffff},
            *mask = masks;
    do {
      if((a = s->read(s)) < 0)
        return (s->c = -1);
      mask++;
    } while((z = joqe_utf8(a, z, &cp))> 0);

    if(!(cp & (mask[0]-mask[-1])) && mask[-1]) {
      // Overlong encoding
      cp = UTF_REPLACEMENT;
    }

    if((cp&0xf800) == 0xd800) {
      // UTF-16 surrogates, invalid codepoints.
      cp = UTF_REPLACEMENT;
    }

    if(z < 0) {
      /* "unshift" last character, it's safe to do so here. It'll be
       * discarded when reading another character at the earliest. */
      if(z < -1) s->b--;
      cp = UTF_REPLACEMENT;
    }
#endif
  }

  return joqe_lex_source_push(s, cp);
}

int
joqe_lex_source_shift(joqe_lex_source *s)
{
  int c = s->c;
  joqe_lex_source_read(s);
  return c;
}


static int
read_eof (joqe_lex_source *s)
{
  return -1;
}
static void
destroy_eof (joqe_lex_source *s)
{
}

static int
fill_buffers_fd (joqe_lex_source *s)
{
  while(s->b >= s->e) {
    int m = s->e&BUFMASK;
    struct iovec iov[2] = {
      { &s->u.buf[m], BUFSZ-m },
      { &s->u.buf[0], m }
    };
    int r = readv(s->i, iov, 2);

    if(r <= 0) {
      if (r) {
        joqe_yyerror(0, "Read error");
      }
      return 0;
    }
    s->e += r;
  }
  return s->e - s->b;
}

static int
read_fd (joqe_lex_source *s)
{
  if(fill_buffers_fd(s))
    return s->u.ubuf[s->b++&BUFMASK];
  return -1;
}

static void
destroy_fd (joqe_lex_source *s)
{
  s->read = read_eof;
  s->destroy = destroy_eof;
  free(s->u.buf);
}

joqe_lex_source
joqe_lex_source_fd(int fd)
{
  joqe_lex_source s = {fd};
  int bom;

  s.u.buf = malloc(BUFSZ);

  // fill the buffers
  if(!fill_buffers_fd(&s)) {
    destroy_fd(&s);
  } else {
    s.f = detect_byte_order(s.u.buf, s.e, &bom);
    s.b = bom;
    s.read = read_fd;
    s.destroy = destroy_fd;

    joqe_lex_source_read(&s);
  }

  return s;
}

static void
destroy_file(joqe_lex_source *s)
{
  destroy_fd(s);
  close(s->i);
}

joqe_lex_source
joqe_lex_source_file(const char *path)
{
  int fd = open(path, O_RDONLY);
  if(fd < 0) {
    joqe_lex_source s = {
      .read = read_eof,
      .destroy = destroy_eof
    };
    return s;
  }

  joqe_lex_source s = joqe_lex_source_fd(fd);
  s.destroy = destroy_file;
  return s;
}

static int
read_string (joqe_lex_source *s)
{
  if(s->b<s->e)
    return s->u.us[s->b++];
  return -1;
}
static void
destroy_string (joqe_lex_source *s)
{
  s->read = read_eof;
  s->destroy = destroy_eof;
  // nothing else to do here.
}

joqe_lex_source
joqe_lex_source_string(const char *s)
{
  joqe_lex_source src = {};
  int bom;
  src.u.s = s;

  src.e = strlen(s);
  src.f = detect_byte_order(s, src.e, &bom);
  src.b = bom;

  src.read = read_string;
  src.destroy = destroy_string;

  joqe_lex_source_read(&src);
  return src;
}

joqe_lex_source
joqe_lex_source_buffer(const char *data, int len)
{
  joqe_lex_source src = {};
  int bom;
  src.u.s = data;

  src.e = len;
  src.f = detect_byte_order(data, src.e, &bom);
  src.b = bom;

  src.read = read_string;
  src.destroy = destroy_string;

  joqe_lex_source_read(&src);
  return src;
}

static int
read_stringarray (joqe_lex_source *s)
{
  if(s->b<s->e) {
    return s->u.uss[0][s->b++];
  } else while(0 <-- s->i) {
    if(++(s->u.ss)) {
      s->e = strlen(s->u.ss[0]);
      s->b = 0;
      // s->f remains unchanged.
      return ' '; // assume one space between strings.
    }
  }
  return -1;
}

joqe_lex_source
joqe_lex_source_stringarray(int i, char * const *ss)
{
  int bom;
  joqe_lex_source src = {};
  src.u.ss = ss;

  src.i = i;
  src.e = strlen(ss[0]);
  src.f = detect_byte_order(ss[0], src.e, &bom);
  src.b = bom;

  src.read = read_stringarray;
  src.destroy = destroy_string;

  joqe_lex_source_read(&src);
  return src;
}
