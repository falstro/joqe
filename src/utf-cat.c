#include "lex-source.h"
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* This is a simple program to help stress test the UTF parser. */

static int err = 0;
int joqe_yyerror(void *build, const char *msg)
{
  fprintf(stderr, "%s\n", msg);
  err = 1;
  return 0;
}

int
main(int argc, char **argv)
{
  char* defargv[] = {argv[0], "-"};
  int i = 0;
  if(argc < 2) {
    argv = defargv;
    argc = 2;
  }

  for(i = 1; i < argc; ++i) {
    joqe_lex_source in;
    if(argv[i][0] == '-' && !argv[i][1]) {
      in = joqe_lex_source_fd(0);
    } else {
      in = joqe_lex_source_file(argv[i]);
    }

    int c;
    while((c = joqe_lex_source_shift(&in)) >= 0) {
      putchar(c);
    }
    in.destroy(&in);
  }
  return 0;
}
