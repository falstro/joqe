#ifndef __JOQE_BUILD_H__
#define __JOQE_BUILD_H__

#include <stdint.h>
#include "hopscotch.h"

typedef struct joqe_slab {
  struct joqe_slab *nxt;
  int   mark;
  int   write;
  char  base[];
} joqe_slab;

typedef struct joqe_build {
  joqe_lex_source     src;
  joqe_ast_construct  root;

  hopscotch   interned;

  int         mode;
  uint32_t    hash;
  uint32_t    block;

  joqe_slab  *first;
  joqe_slab  *current;
} joqe_build;


joqe_build  joqe_build_init(joqe_lex_source src);
void        joqe_build_destroy(joqe_build *build);
int         joqe_build_appendstring(joqe_build* build, int c);
const char* joqe_build_closestring(joqe_build* build);
void        joqe_build_cancelstring(joqe_build* build);

#endif /* idempotent include guard */
