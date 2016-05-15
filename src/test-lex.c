#include "ast.h"
#include "lex-source.h"
#include "build.h"
#include "joqe.tab.h"
#include "lex.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int err = 0;
int joqe_yyerror(struct joqe_build *build, const char *msg)
{
  fprintf(stderr, "%s\n", msg);
  err = 1;
  return 0;
}

int
main(void)
{
  JOQE_YYSTYPE yylval;
  joqe_build build = joqe_build_init(
    joqe_lex_source_string(
      "123 0123 0xabc \"\\u000a\" >= 3.25/ \n"
      "//[]and abc andab\"d\\\"e\\\"f\" nuke not\""
    )
  );
  assert(joqe_yylex(&yylval, &build) == INTEGER);
  assert(yylval.integer == 123);
  assert(joqe_yylex(&yylval, &build) == INTEGER);
  assert(yylval.integer == 0123);
  assert(joqe_yylex(&yylval, &build) == INTEGER);
  assert(yylval.integer == 0xabc);
  assert(joqe_yylex(&yylval, &build) == STRING);
  assert(0 == strcmp(yylval.string,"\n"));
  assert(joqe_yylex(&yylval, &build) == GEQ);
  assert(joqe_yylex(&yylval, &build) == REAL);
  assert(yylval.real == 3.25);
  assert(joqe_yylex(&yylval, &build) == '/');
  assert(joqe_yylex(&yylval, &build) == SLASH2);
  assert(joqe_yylex(&yylval, &build) == '[');
  assert(joqe_yylex(&yylval, &build) == ']');
  assert(joqe_yylex(&yylval, &build) == AND);
  assert(joqe_yylex(&yylval, &build) == IDENTIFIER);
  assert(0 == strcmp(yylval.string,"abc"));
  assert(joqe_yylex(&yylval, &build) == IDENTIFIER);
  assert(0 == strcmp(yylval.string,"andab"));
  assert(joqe_yylex(&yylval, &build) == STRING);
  assert(0 == strcmp(yylval.string,"d\"e\"f"));
  assert(joqe_yylex(&yylval, &build) == IDENTIFIER);
  assert(0 == strcmp(yylval.string,"nuke"));
  assert(joqe_yylex(&yylval, &build) == NOT);
  assert(joqe_yylex(&yylval, &build) == INVALID_STRING);

  joqe_build_destroy(&build);

  joqe_lex_source s;
  s = joqe_lex_source_buffer("\x00i", 2);
  assert(s.f == JOQE_LEX_UTF16);
  assert(s.c == 'i');
  s = joqe_lex_source_buffer("j\x00", 2);
  assert(s.f == (JOQE_LEX_UTF16|JOQE_LEX_MB_LE));
  assert(s.c == 'j');
  s = joqe_lex_source_buffer("\x00\x00\x00k", 4);
  assert(s.f == (JOQE_LEX_UTF32));
  assert(s.c == 'k');
  s = joqe_lex_source_buffer("l\x00\x00\x00", 4);
  assert(s.f == (JOQE_LEX_UTF32|JOQE_LEX_MB_LE));
  assert(s.c == 'l');
  s = joqe_lex_source_buffer("\x00m\x00n", 4);
  assert(s.f == (JOQE_LEX_UTF16));
  assert(s.c == 'm');
  assert(joqe_lex_source_read(&s) == 'n');
  s = joqe_lex_source_buffer("o\x00p\x00", 4);
  assert(s.f == (JOQE_LEX_UTF16|JOQE_LEX_MB_LE));
  assert(s.c == 'o');
  assert(joqe_lex_source_read(&s) == 'p');

  s = joqe_lex_source_buffer("\xff\xfeq\x00", 4);
  assert(s.f == (JOQE_LEX_UTF16|JOQE_LEX_MB_LE));
  assert(s.c == 'q');
  s = joqe_lex_source_buffer("\xfe\xff\x00r", 4);
  assert(s.f == JOQE_LEX_UTF16);
  assert(s.c == 'r');
  s = joqe_lex_source_buffer("\x00\x00\xfe\xff\x00\x00\x00s", 8);
  assert(s.f == JOQE_LEX_UTF32);
  assert(s.c == 's');
  s = joqe_lex_source_buffer("\xff\xfe\x00\x00t\x00\x00\x00", 8);
  assert(s.f == (JOQE_LEX_UTF32|JOQE_LEX_MB_LE));
  assert(s.c == 't');

  build = joqe_build_init(
    joqe_lex_source_string(
      "\"\\uffff\" \"\\u8437\" \"\\ud800\\udc00\" \"x\\ud800y\""
    )
  );
  assert(joqe_yylex(&yylval, &build) == STRING);
  assert(0 == strcmp(yylval.string,"\xef\xbf\xbf"));
  assert(joqe_yylex(&yylval, &build) == STRING);
  assert(0 == strcmp(yylval.string,"\xe8\x90\xb7"));
  assert(joqe_yylex(&yylval, &build) == STRING);
  assert(0 == strcmp(yylval.string,"\xf0\x90\x80\x80"));
  assert(joqe_yylex(&yylval, &build) == INVALID_STRING);
  joqe_build_destroy(&build);

  build = joqe_build_init(
    joqe_lex_source_string(
      "\"\\ud800 \"x\" \"\xc0\x8a\""
    )
  );
  assert(joqe_yylex(&yylval, &build) == INVALID_STRING);
  assert(joqe_yylex(&yylval, &build) == STRING);
  assert(0 == strcmp(yylval.string,"x"));
  assert(joqe_yylex(&yylval, &build) == STRING);
  assert(0 == strcmp(yylval.string,"\xef\xbf\xbd")); // replacement character
  joqe_build_destroy(&build);
  return err;
}
