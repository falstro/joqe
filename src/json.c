#include "json.h"
#include "ast.h"
#include "lex-source.h"
#include "build.h"
#include "joqe.tab.h"
#include "lex.h"

#include <stdlib.h>
#include <stdio.h>

int joqe_yyerror(joqe_build *b, const char *msg);

static
int json_element (int           token,
                  JOQE_YYSTYPE *yylval,
                  joqe_build   *b,
                  joqe_node    *n);

static int
json_object(JOQE_YYSTYPE *yylval, joqe_build *b, joqe_node *n)
{
  n->type |= joqe_type_none_object;
  int token = joqe_yylex(yylval, b);
  if(token == '}')
    return 0;
  goto first;
  do {
    token = joqe_yylex(yylval, b);
    first: if(STRING != token) {
      //expected string or '}', not a string so check '}'
      //this allows {"a":"b",}
      break;
    }
    const char *key = yylval->string;

    if(':' != joqe_yylex(yylval, b)) {
      joqe_yyerror(b, "expected ':'");
      return -1;
    }

    joqe_nodels l = {{}, {joqe_type_string_none, .k = {.key = key}}},
               *ls;

    token = joqe_yylex(yylval, b);
    if((token = json_element(token, yylval, b, &l.n)))
      return token;

    *(ls = calloc(1, sizeof(*ls))) = l;
    joqe_list_append((joqe_list**)&n->u.ls, &ls->ll);
  } while((token = joqe_yylex(yylval, b)) == ',');
  if(token == '}')
    return 0;

  joqe_yyerror(b, "expected '}'");
  return token?token:-1;
}

static int
json_array(JOQE_YYSTYPE *yylval, joqe_build *b, joqe_node *n)
{
  int token, idx = 0;
  n->type |= joqe_type_none_array;
  do {
    joqe_nodels l = {{}, {joqe_type_int_none, .k = {.idx = idx++}}},
               *ls;

    token = joqe_yylex(yylval, b);
    if(token == ']')
      // allows [] and [123,]
      break;
    if((token = json_element(token, yylval, b, &l.n)))
      return token;

    *(ls = calloc(1, sizeof(*ls))) = l;
    joqe_list_append((joqe_list**)&n->u.ls, &ls->ll);
  } while((token = joqe_yylex(yylval, b)) == ',');
  if(token == ']')
    return 0;

  joqe_yyerror(b, "expected ']'");
  return token?token:-1;
}

static int
json_element(int token, JOQE_YYSTYPE *yylval, joqe_build *b, joqe_node *n)
{
  int mult = 1;
  switch(token)
  {
    case '{': return json_object(yylval, b, n); break;
    case '[': return json_array(yylval, b, n); break;
    case STRING:
      n->type |= joqe_type_none_string;
      n->u.s = yylval->string;
      break;
    case INTEGER:
      n->type |= joqe_type_none_integer;
      n->u.i = yylval->integer;
      break;
    case REAL:
      n->type |= joqe_type_none_real;
      n->u.d = yylval->real;
      break;
    case _TRUE:
      n->type |= joqe_type_none_true;
      n->u.i = 1;
      break;
    case _FALSE:
      n->type |= joqe_type_none_false;
      n->u.i = 0;
      break;
    case _NULL:
      n->type |= joqe_type_none_null;
      break;
    case '-': mult = -1; /* fall through */
    case '+':
      token = joqe_yylex(yylval, b);
      if(token == INTEGER) {
        n->type |= joqe_type_none_integer;
        n->u.i = mult*yylval->integer;
      } else if (token == REAL) {
        n->type |= joqe_type_none_real;
        n->u.d = mult*yylval->real;
      } else {
        joqe_yyerror(b, "unexpected plus/minus non-numeric");
        return -1;
      }
      break;
    case 0:
      joqe_yyerror(b, "unexpected end of input");
      return -1;
    default: {
      char message[] = "unexpected token 0x    ";
      sprintf(&message[19], "%02x", token);
      joqe_yyerror(b, message);
    } return token;
  }
  return 0;
}

static int
json_construct (joqe_ast_construct *c,
                joqe_node *nn, joqe_node *cc,
                joqe_result *r)
{
  if(!nn) {
    joqe_result r = {};
    joqe_result_clear_node(c->u.node, &r);
    joqe_result_destroy(&r);
    return 0;
  }

  joqe_nodels *ls = calloc(1, sizeof(*ls));
  ls->n = joqe_result_copy_node(&c->u.node);
  joqe_list_append((joqe_list**)r, &ls->ll);
  return 1;
}

int
joqe_json (joqe_build *b)
{
  JOQE_YYSTYPE yylval;

  joqe_node n = {};
  int r = json_element(joqe_yylex(&yylval, b), &yylval, b, &n);
  if(0 == r) {
    joqe_ast_construct c = {json_construct, {.node = n}};
    b->root = c;
  }
  return r;
}
