#include "ast.h"
#include "lex-source.h"
#include "build.h"
#include "joqe.tab.h"
#include "lex.h"
#include "utf.h"
#include "json.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

const char *argv0;

static int q = 0;

int
joqe_yyerror(joqe_build *build, const char *msg)
{
  if(!q) {
    if(build)
      fprintf(stderr, "%s: %s:%d:%d: %s\n",
              argv0, build->src.name ? build->src.name : "<none>",
              build->src.line + 1, build->src.col,
              msg);
    else
      fprintf(stderr, "%s: %s\n", argv0, msg);
  }
  return 0;
}

typedef struct {
  int         raw;
  int         pp;
  int         ind;
  const char *nl;
  int         nllen;
  int         ascii;
  int         array;
  const char *separator;
} config;

void dump(joqe_node n, int lvl, config *c);

void
dump_kvs(joqe_nodels *ns, int lvl, config *c)
{
  joqe_nodels *ls;
  int align = 0;
  if(c->pp > 1) {
    if((ls = ns)) do {
      int l = JOQE_TYPE_KEY(ls->n.type) == joqe_type_string_none ?
        strlen(ls->n.k.key) : 0;
      if (l > align)
        align = l;
    } while((ls = (joqe_nodels*)ls->ll.n) != ns);
  }

  const char *k;
  const char *nl = c->nl;
  int ind = c->ind*lvl + c->nllen;
  int off, minoff = c->pp ? 1 : 0;
  if((ls = ns)) {
    if(ls->n.type == joqe_type_ref_cnt)
      ls = (joqe_nodels*) ls->ll.n;
    goto first;
    do {
      printf(",");
      first:
        k = JOQE_TYPE_KEY(ls->n.type) == joqe_type_string_none ?
          ls->n.k.key : "";
        off = minoff + (align?align-strlen(k):0);
        if(c->raw > 1) printf("%-*s%s:%*s", ind, nl, k, -off, "");
        else printf("%-*s\"%s\":%*s", ind, nl, k, -off, "");
      dump(ls->n, lvl, c);
    } while((ls = (joqe_nodels*)ls->ll.n) != ns);
  }
}
void
dump_vs(joqe_nodels *ns, int lvl, config *c)
{
  joqe_nodels *ls;
  int ind = c->array > 1 || (c->array && lvl <= 1) ? 0 : c->ind*lvl+c->nllen;
  if((ls = ns)) {
    if(ls->n.type == joqe_type_ref_cnt)
      ls = (joqe_nodels*) ls->ll.n;
    goto first;
    do {
      printf("%c", c->array ? *c->separator : ',');
      first: if(ind) printf("%-*s", ind, c->nl);
      dump(ls->n, lvl, c);
    } while((ls = (joqe_nodels*)ls->ll.n) != ns);
  }
}

void
dumpstring(const char *s, config *c)
{
  putchar('\"');
  for(;*s;s++) {
    switch(*s) {
      case '\"': putchar('\\'); putchar('\"'); break;
      case '\\': putchar('\\'); putchar('\\'); break;
      case '\b': putchar('\\'); putchar('b'); break;
      case '\f': putchar('\\'); putchar('f'); break;
      case '\n': putchar('\\'); putchar('n'); break;
      case '\r': putchar('\\'); putchar('r'); break;
      case '\t': putchar('\\'); putchar('t'); break;
      default:
        if((*s & 0x80) && c->ascii) {
          uint32_t cp = 0;
          int z = 0;

          while((z = joqe_utf8(*s,z,&cp)))
            s++;

          if((cp & 0xffff) != cp) {
            cp -= 0x10000;
            uint32_t l = (cp & 0x3ff) | 0xdc00,
                     h = ((cp>>10) & 0x3ff) | 0xd800;
            printf("\\u%04x\\u%04x", h, l);
          } else {
            printf("\\u%04x", cp);
          }
        } else if((*s & 0x1f) == *s) {
          printf("\\u00%x", *s);
        } else {
          putchar(*s);
        }
    }
  }
  putchar('\"');
}

void
dump(joqe_node n, int lvl, config *c)
{
  if(n.type == joqe_type_ref_cnt)
    return; // only happens on an empty list with a ref count, shouldn't happen
  switch(JOQE_TYPE_VALUE(n.type)) {
    case joqe_type_broken:
      printf("broken"); break;
    case joqe_type_none_string:
      if(c->raw > 1 || (c->raw && !lvl))
        printf("%s", n.u.s);
      else
        dumpstring(n.u.s, c);
      break;
    case joqe_type_none_integer:
      printf("%ld", n.u.i); break;
    case joqe_type_none_real:
      printf("%f", n.u.d); break;
    case joqe_type_none_true:
      printf("true"); break;
    case joqe_type_none_false:
      printf("false"); break;
    case joqe_type_none_null:
      printf("null"); break;
#define PP c->ind*lvl+c->nllen, c->nl
    case joqe_type_none_object:
      if(n.u.ls) {
        printf("{"); dump_kvs(n.u.ls, lvl+1, c);
        printf("%-*s}", PP);
      } else {
        printf("{}");
      }
      break;
    case joqe_type_none_array:
      if(c->array > 1 || (c->array && !lvl)) {
        if(n.u.ls)
          dump_vs(n.u.ls, lvl+1, c);
      } else {
        if(n.u.ls) {
          printf("["); dump_vs(n.u.ls, lvl+1, c);
          printf("%-*s]", PP);
        } else {
          printf("[]");
        }
      }
      break;
#undef PP
    default:
      printf("unknown %d", JOQE_TYPE_VALUE(n.type));
  }
}

int
fail(const char *msg, ...)
{
  va_list ap;
  va_start(ap, msg);

  fprintf(stderr, "%s: ", argv0);
  vfprintf(stderr, msg, ap);
  fprintf(stderr, "\n");

  va_end(ap);
  return 1;
}

void
usage(FILE *out)
{
  fprintf(out, "Usage:\n"
    "\t%s [options] [--] [expression [file ...]]\n"
    "\t%s [options] -f expression-file [--] [file ...]\n"
    "\t%s [options] -P [--] [expression ...]\n"
    "\n"
    "Expression interpretation:\n"
    "\t-f           Read expression from a file.\n"
    "\n"
    "Options:\n"
    "\t-I INDENT    Indent each level by INDENT number of spaces. Implies -F.\n"
    "\t-a           Limit output to ASCII only, encode all 8-bit and multibyte\n"
    "\t             characters using escape sequences (e.g. \\u12cd)\n"
    "\t-F           Format/pretty-print the output. Use twice to align object\n"
    "\t             values into columns.\n"
    "\t-q           Quiet, fail silently on parsing errors.\n"
    "\t-r           Print raw (don't quote) strings at the top level. Use\n"
    "\t             twice to suppress quoting of all strings.\n"
    "\t-A           Arrays at top level will be 'shell lists' without brackets\n"
    "\t             and using space as separators. Use with double -r to get\n"
    "\t             unquoted strings.\n"
    "\t-S SEP       Use SEP separator instead of space for shell lists. This\n"
    "\t             option implies -A.\n"
    "\t-h           Print this help.\n"
    "\n", argv0, argv0, argv0);
}

int
main(int argc, char **argv)
{
  int i, opt, r = 0;
  const char* expfile = 0;
  config c = {.separator = " "};

  argv0 = argv[0];

  while((opt = getopt(argc, argv, "hI:af:FqrAS:")) != -1) switch(opt) {
    case '?': usage(stderr); return 1;
    case 'h': usage(stdout); return 0;
    case 'f': expfile = optarg; break;
    case 'I': {
      char *e;
      c.ind = strtol(optarg, &e, 0);
      if(*e || !*optarg)
        return fail("indentation must be numeric: %s", optarg);
    } break;
    case 'q': q = 1; break;
    case 'F': c.pp++; break;
    case 'r': c.raw++; break;
    case 'a': c.ascii++; break;
    case 'A': c.array++; break;
    case 'S': c.array = c.array ? c.array : 1; c.separator = optarg; break;
  }
  i = optind;

  if(c.ind && !c.pp) c.pp = 1;
  else if(!c.ind && c.pp) c.ind = 4;
  if(!c.nl) c.nl = c.pp ? "\n" : "";
  c.nllen = strlen(c.nl);

  joqe_ast_construct *cst = 0;
  joqe_build exp = {};
  if(expfile || (i < argc)) {
    joqe_lex_source src;
    if(expfile) {
      src = joqe_lex_source_file(expfile);
    } else {
      src = joqe_lex_source_string(argv[i++]);
      src.name = "expression #1";
    }

    exp = joqe_build_init(src);
    r = joqe_yyparse(&exp);
    exp.src.destroy(&exp.src);
    if(r) {
      joqe_build_destroy(&exp);
      return 1;
    } else cst = &exp.root;
  }

  joqe_node nullnode = {joqe_type_none_null};
  joqe_ctx nullctx = {NULL, &nullnode};

  do {
    joqe_build bdoc;
    joqe_result rdoc = {};
    joqe_result jr = {};
    const char *fname;
    if(i >= argc || 0 == strcmp("-", argv[i])) {
      fname = i >= argc ? "<stdin>" : argv[i];
      bdoc = joqe_build_init(joqe_lex_source_fd(0));
      r = joqe_json(&bdoc);
      bdoc.src.destroy(&bdoc.src);
    } else {
      fname = argv[i];
      bdoc = joqe_build_init(joqe_lex_source_file(argv[i]));
      r = joqe_json(&bdoc);
      bdoc.src.destroy(&bdoc.src);
    }

    if(r) {
      if (!q) fprintf(stderr, "%s: %s: parse failed\n", argv0, fname);
      continue;
    }

    bdoc.root.construct(&bdoc.root, &nullnode, &nullctx, &rdoc);

    if(cst) {
      joqe_ctx rootcontext = {NULL, &rdoc.ls->n};
      cst->construct(cst, rootcontext.node, &rootcontext, &jr);
    } else {
      joqe_nodels *i;
      if((i = rdoc.ls)) do {
        joqe_nodels *nls = joqe_result_alloc_node(&jr);
        nls->n = joqe_result_copy_node(&i->n);
        joqe_list_append((joqe_list**)&jr.ls, &nls->ll);
      } while((i = (joqe_nodels*)&i->ll) != rdoc.ls);
    }

    joqe_nodels *ls;
    if((ls = jr.ls)) do {
      dump(ls->n, 0, &c);
      printf("\n");
    } while((ls = (joqe_nodels*)ls->ll.n) != jr.ls);

    joqe_result_destroy(&jr);
    joqe_result_destroy(&rdoc);

    joqe_build_destroy(&bdoc);
  } while(++i < argc);

  joqe_build_destroy(&exp);
  return r;
}
