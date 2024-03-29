#include "ast.h"
#include "lex-source.h"
#include "build.h"
#include "joqe.tab.h"
#include "lex.h"
#include "utf.h"

#include <stdio.h>

#include <string.h>
#include <math.h>

#define SINGLES \
    '|': case '[': case ']': case '{': case '}': case '(': case ')': \
    case '+': case '-': case '*': case '%': case '=': case ',': case -1
#define DOUBLES '/': case '.': case ':'
#define WHITE '\r': case '\n': case '\t': case '\v': case '\f': case ' '
#define DIGITS '0': case '1': case '2': case '3': case '4': \
          case '5': case '6': case '7': case '8': case '9'

#define NONIDENTS '<': case '>': case '\\': case '"': case '\'': \
    case SINGLES: case DOUBLES: case WHITE: case ';': case '~': case '?'

typedef struct
{
  JOQE_YYSTYPE *yylval;
  joqe_build *builder;
}
lexparam;

static inline int
consume(lexparam l)
{
  return joqe_lex_source_read(&l.builder->src);
}
static inline int
peek(lexparam l)
{
  return l.builder->src.c;
}

static int
ident(lexparam l)
{
  int c = peek(l);
  while(c) switch(c) {
    case NONIDENTS:
      c = 0; break;
    default:
      if (c < 48) {
        c = 0; break;
      }
      joqe_build_appendstring(l.builder, c);
      c = consume(l);
      break;
  }

  if(!(l.yylval->string = joqe_build_closestring(l.builder))) {
    l.yylval->integer = INVALID_OVERLONG;
    return INVALID_STRING;
  }
  return IDENTIFIER;
}

static int
nonident(int c)
{
  switch(c) {
    case NONIDENTS:
      return 1;
    default:
      if(c < 48)
        return 1;
  }
  return 0;
}

static int
keyword(lexparam l, const char *word, int token, int offset, int len)
{
  int c = peek(l);
  for(;offset < len;offset++) {
    if (c != word[offset])
      break;
    joqe_build_appendstring(l.builder, c);
    c = consume(l);
  }
  if(offset == len && nonident(c)) {
    joqe_build_cancelstring(l.builder);
    return token;
  }

  return ident(l);
}

static int
hex2dec(int c)
{
  if(c >= '0' && c <= '9')
    return c-'0';
  if(c >= 'a' && c <= 'f')
    return c-'a'+10;
  if(c >= 'A' && c <= 'F')
    return c-'A'+10;
  return -1;
}
static int
string(lexparam l, int delimiter)
{
  int c = peek(l);
  joqe_lex_source *s = &l.builder->src;
  while(c >= 0)
  {
    if(c == delimiter)
    {
      consume(l);
      if(!(l.yylval->string = joqe_build_closestring(l.builder))) {
        l.yylval->integer = INVALID_OVERLONG;
        return INVALID_STRING;
      }
      return STRING;
    }

    if(c < 0x20) {
      l.yylval->integer = INVALID_CONTROL_CHARACTER;
      return INVALID_STRING;
    }

    if(c == '\\')
    {
      c = consume(l);
      switch(c) {
        case 'a': c = '\a'; break; case 'b': c = '\b'; break;
        case 'f': c = '\f'; break; case 'n': c = '\n'; break;
        case 'r': c = '\r'; break; case 't': c = '\t'; break;
        case 'u': {
          uint32_t u = 0;
          int z = 0;
          do {
            if(z) {
              if(consume(l) != '\\') {
                l.yylval->integer = INVALID_UNICODE_SURROGATE;
                return INVALID_STRING;
              } else if(consume(l) != 'u') {
                l.yylval->integer = INVALID_UNICODE_SURROGATE;
                return INVALID_STRING;
              }
            }
            int32_t A = hex2dec(consume(l));
            int32_t B = hex2dec(consume(l));
            int32_t C = hex2dec(consume(l));
            int32_t D = hex2dec(c = consume(l));
            if((A|B|C|D) < 0 || !c) {
              l.yylval->integer = INVALID_UNICODE_ESCAPE;
              return INVALID_STRING;
            }

            z = joqe_utf16(A<<4|B, C<<4|D, z, &u);
          } while(z);

          c = joqe_lex_source_push(s, u);
        } break;
        default:
          if(c < 0x20) {
            l.yylval->integer = INVALID_UNICODE_ESCAPE;
            return INVALID_STRING;
          }
          break;
      }
    }

    if(joqe_build_appendstring(l.builder, c)) {
      if((l.yylval->string = joqe_build_closestring(l.builder))) {
        l.builder->mode = delimiter;
        return PARTIALSTRING;
      }
    }
    c = consume(l);
  }
  l.yylval->integer = INVALID_END_OF_INPUT;
  return INVALID_STRING;
}

static int
digit(int c)
{
  int d;
  if((d=c-'0') < 0 || d > 10)
    if((d=10+c-'a') < 10 || d > 15)
      if((d=10+c-'A') < 10 || d > 15)
        d = -1;
  return d;
}

static int
number(lexparam l)
{
  int token = INTEGER;

  int c = peek(l);
  int base = 10;
  int state = 0;
  int64_t ival = 0;
  int fcnt = 0;
  int exp = 0;
  int expmult = 1;

  /* multi-digit numbers can't start with 0 in json, so parsing numbers
   * starting with 0 and 0x as octal and hex respectively does not
   * impede our ability to parse valid json. */
  for(; c >= 0; c = consume(l)) {
    int v = digit(c);
    top: if(v < 0 || v >= base) switch(c) {
      // N.B. starting with a . is not legal JSON.
      case '.': if(state < 3) {
          if(state == 1) base = 10;
          state = 3; token = REAL;
        } else goto end; break;
      case 'e':
      case 'E': if(state < 4) { state = 4; token = REAL; } else goto end; break;
      case 'x': if(state == 1) { base = 16; state = 2; } else goto end; break;
      case '-': expmult = -1; /* fall through */
      case '+': if(state == 4) { state = 5; } else goto end; break;
      default: goto end;
    } else switch(state) {
      case 0: if(v == 0) { state = 1; base = 8; break; } /* fall through */
      case 1: state = 2; goto top;
      case 3: fcnt++; /* fall through */
      case 2: ival = ival*base + v;
              break;
      case 4: state = 5; /* fall through */
      case 5: exp = base*exp + v; break;
    }
  }

  end: switch(token) {
    case REAL:
      l.yylval->real = ival*pow(base, expmult*exp-fcnt);
      break;
    case INTEGER:
      // TODO range check
      l.yylval->integer = ival;
      break;
  }
  return token;
}

int
c_comment(lexparam l)
{
  int c = consume(l);
  while(c >= 0) {
    int oc = c;
    c = consume(l);
    if(oc == '*' && c == '/') {
      return consume(l);
    }
  }
  // TODO error, end of file within comment.
  return c;
}

int
joqe_yylex(JOQE_YYSTYPE *yylval, joqe_build *build)
{
  lexparam l = {yylval, build};
  if (build->mode) {
    int m = build->mode;
    build->mode = 0;

    switch(m) {
      case '"': case '\'':
        return string(l, m);
    }
  }
  int n, c = peek(l);

  while(1) switch(c) {
    case SINGLES:
      consume(l);
      return c;
    case '"': case '\'':
      consume(l);
      return string(l, c);
    case 'a': return keyword(l, "and", AND, 0, 3);
    case 'o': return keyword(l, "or", OR, 0, 2);
    case 't': return keyword(l, "true", _TRUE, 0, 4);
    case 'f': return keyword(l, "false", _FALSE, 0, 5);
    case 'n':
      joqe_build_appendstring(l.builder, c);
      switch(consume(l)) {
        case 'o': return keyword(l, "not", NOT, 1, 3);
        case 'u': return keyword(l, "null", _NULL, 1, 4);
        default:
          return ident(l);
      }
    case DIGITS: return number(l);
    case '/': {
      n = consume(l);
      if(n == c) {
        int slashcount = 0;
        do {
          slashcount++;
          n = consume(l);
        } while(n == c);
        yylval->integer = slashcount;
        return SLASH;
      } else if(n == '*') {
        c = c_comment(l);
      } else {
        return c;
      }
    } break;
    case '.': if(c == consume(l)) {consume(l); return DOT2;} else return c;
    case ':': if(c == consume(l)) {consume(l); return C2;} else return c;
    case '<': if('=' == consume(l)) {consume(l); return LEQ;} else return c;
    case '>': if('=' == consume(l)) {consume(l); return GEQ;} else return c;
    case '!': if('=' == consume(l)) {consume(l); return NEQ;} else return c;
    case WHITE:
      c = consume(l);
      break;
    default:
      if(c < 0x20)
        //TODO error, other characters 0x1f or below are not valid.
        return 0;
      return ident(l);
  }

  return 0;
}
