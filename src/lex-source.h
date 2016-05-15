#ifndef __JOQE_LEX_SOURCE_H__
#define __JOQE_LEX_SOURCE_H__

#define JOQE_LEX_UTF16  0x01
#define JOQE_LEX_UTF32  0x02
#define JOQE_LEX_MB_LE  0x04

#include <stdint.h>

typedef struct joqe_lex_source
{
  int i,b,e;
  int c,f;
  unsigned char mb[5], mbi;
  union {
    char * const *ss;
    const char *s;
    char *buf;
    unsigned char * const *uss;
    const unsigned char *us;
    unsigned char *ubuf;
  } u;

  int (*read)(struct joqe_lex_source*);
  void (*destroy)(struct joqe_lex_source*);
}
joqe_lex_source;

joqe_lex_source joqe_lex_source_fd(int fd);
joqe_lex_source joqe_lex_source_file(const char *path);
joqe_lex_source joqe_lex_source_string(const char *s);
joqe_lex_source joqe_lex_source_buffer(const char *buffer, int len);
joqe_lex_source joqe_lex_source_stringarray(int i, char * const *ss);

int             joqe_lex_source_push (joqe_lex_source *s,
                                      uint32_t codepoint);
int             joqe_lex_source_read (joqe_lex_source *s);
int             joqe_lex_source_shift(joqe_lex_source *s);

#endif /* idempotent include guard */
