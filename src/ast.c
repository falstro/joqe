#include "ast.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <assert.h>

#define IMPLODE 0, 0, 0

static inline void
joqe_result_append(joqe_result *r, joqe_nodels *n)
{
  joqe_list_append((joqe_list**)&r->ls, &n->ll);
}

joqe_nodels*
joqe_result_alloc_node (joqe_result *r)
{
  joqe_nodels *n;
  if(r->freels) {
    n = (joqe_nodels*)
      joqe_list_detach((joqe_list**)&r->freels, r->freels->ll.n);
  } else {
    n = malloc(sizeof(joqe_nodels));
  }

  memset(n, 0, sizeof(*n));
  return n;
}

joqe_node
joqe_result_copy_node (joqe_node *n)
{
  switch(JOQE_TYPE_VALUE(n->type)) {
    case joqe_type_none_object:
    case joqe_type_none_array:
    case joqe_type_none_stringls:
      if(n->u.ls) {
        if(n->u.ls->n.type == joqe_type_ref_cnt) {
          n->u.ls->n.u.i++;
        } else {
          // No ref count object implies a ref count of 1. This is the
          // second reference.
          joqe_nodels refCnt = {.n = {joqe_type_ref_cnt, .u = {.i = 2}}},
                     *e = malloc(sizeof(*e));

          *e = refCnt;
          joqe_list_append((joqe_list**)&n->u.ls, &e->ll);
          n->u.ls = e; // (joqe_nodels*) n->u.ls->ll.p;
        }
      }
  }
  joqe_node copy = *n;
  return copy;
}

static void
joqe_result_free_list (joqe_nodels *list, joqe_result *r)
{
  joqe_nodels *i, *next;
  if(!list)
    return;

  if(list->n.type == joqe_type_ref_cnt) {
    if(0<--list->n.u.i)
      return;
  }

  if((i = list)) do {
    next = (joqe_nodels*)i->ll.n;
    // No need to do a proper detach, we're freeing them all.
    i->ll.p = i->ll.n = &i->ll;
    joqe_result_free_node(i, r);
  } while((i = next) != list);
}

void
joqe_result_clear_node (joqe_node n, joqe_result *r)
{
  switch(JOQE_TYPE_VALUE(n.type)) {
    case joqe_type_none_object:
    case joqe_type_none_array:
    case joqe_type_none_stringls: {
      joqe_result_free_list(n.u.ls, r);
      n.u.ls = 0;
    } /* fall through */
  }
}

void
joqe_result_free_node (joqe_nodels *n, joqe_result *r)
{
  assert(n->ll.p == n->ll.n);
  joqe_result_clear_node(n->n, r);
  joqe_list_append((joqe_list**)&r->freels, &n->ll);
}

#ifdef DEBUG_FREE
#include <stdio.h>
#endif
static void
nodels_free(joqe_nodels *ls)
{
  joqe_nodels *i, *nxt, *end;
  if((end = i = ls)) do {
#ifdef DEBUG_FREE
    fprintf(stderr, "freeing: ");
    switch(JOQE_TYPE_KEY(i->n.type)) {
      case joqe_type_string_none:
        fprintf(stderr, "'%s': ", i->n.k.key);
        break;
      case joqe_type_int_none:
        fprintf(stderr, "%d: ", i->n.k.idx);
        break;
      default:
        fprintf(stderr, "none: ");
        break;
    }
    switch(JOQE_TYPE_VALUE(i->n.type)) {
      case joqe_type_none_string:
        fprintf(stderr, "%s\n", i->n.u.s);
        break;
      case joqe_type_none_integer:
        fprintf(stderr, "%d\n", i->n.u.i);
        break;
      case joqe_type_none_real:
        fprintf(stderr, "%g\n", i->n.u.d);
        break;
      case joqe_type_none_object:
        fprintf(stderr, "object\n");
        break;
      case joqe_type_none_array:
        fprintf(stderr, "array\n");
        break;
      case joqe_type_none_stringls:
        fprintf(stderr, "stringls\n");
        break;
      default:
        fprintf(stderr, "broken?\n");
    }
#endif
    nxt = (joqe_nodels*) i->ll.n;
    free(i);
  } while((i = nxt) != end);
}

void
joqe_result_destroy (joqe_result *r)
{
  joqe_result_free_list(r->ls, r);
  nodels_free(r->freels);
}

static joqe_result
joqe_result_push(joqe_result *r)
{
  if(r) {
    joqe_result n = {.freels = r->freels};
    r->freels = 0;
    return n;
  } else {
    joqe_result n = {};
    return n;
  }
}

static void
joqe_result_free_transfer(joqe_result *target, joqe_result *src)
{
  joqe_list_append((joqe_list**)&target->freels, &src->freels->ll);
  src->freels = 0;
}

static void
joqe_result_pop(joqe_result *base, joqe_result *r)
{
  if(base) {
    joqe_result_free_list(r->ls, r);
    joqe_result_free_transfer(base, r);
    base->status |= r->status;
  } else {
    joqe_result_destroy(r);
  }
}

static void
joqe_result_transfer(joqe_result *base, joqe_result *r)
{
  joqe_result_append(base, r->ls);
  r->ls = 0;
}


static joqe_nodels *
result_nodels (joqe_result *r, joqe_type type)
{
  joqe_nodels *ls = joqe_result_alloc_node(r);
  ls->n.type = type;
  joqe_result_append(r, ls);
  return ls;
}

static int
strlscmp(joqe_node l, joqe_node r)
{
  int le, re, e, d, lo = 0, ro = 0;

  assert(JOQE_TYPE_VALUE(l.type) == joqe_type_none_stringls);
  assert(JOQE_TYPE_VALUE(r.type) == joqe_type_none_stringls);

  joqe_nodels *li = l.u.ls, *ri = r.u.ls;
  joqe_nodels *endl = li, *endr = ri;

  joqe_nodels emptyls = {.n = {joqe_type_none_string, .u = {.s = ""}}};
  emptyls.ll.n = emptyls.ll.p = &emptyls.ll;

  if(!li) { endl = li = &emptyls; }
  else if(li->n.type == joqe_type_ref_cnt) { li = (joqe_nodels*)li->ll.n; }
  if(!ri) { endr = ri = &emptyls; }
  else if(ri->n.type == joqe_type_ref_cnt) { ri = (joqe_nodels*)ri->ll.n; }

  le = strlen(li->n.u.s);
  re = strlen(ri->n.u.s);

  do {
    e = min(le-lo, re-ro);
    assert(e >= 0);

    if(e) {
      if((d = strncmp(li->n.u.s + lo, ri->n.u.s + ro, e)))
        return d;
    }

    lo += e;
    ro += e;

    if(lo == le) {
      lo = 0;
      do {
        li = (joqe_nodels*)li->ll.n;
        le = (li == endl) ? -1 : strlen(li->n.u.s);
      } while(le == 0);
    }
    if(ro == re) {
      ro = 0;
      do {
        ri = (joqe_nodels*)ri->ll.n;
        re = (ri == endr) ? -1 : strlen(ri->n.u.s);
      } while(re == 0);
    }
  } while(le >= 0 && re >= 0);

  return le < 0 ? re < 0 ? 0 : -1 : 1;
}

static int
strlsstrcmp(joqe_node l, const char *s)
{
  joqe_nodels ls = {.n = {joqe_type_none_string, .u = {.s = s}}};
  joqe_node r = {joqe_type_none_stringls, .u = {.ls = &ls}};

  ls.ll.n = ls.ll.p = &ls.ll;

  return strlscmp(l, r);
}

static joqe_ast_construct*
ast_construct_alloc()
{
  return calloc(1, sizeof(joqe_ast_construct));
}

static int
ast_construct_free(joqe_ast_construct *c)
{
  if(c) free(c);
  return 0;
}

static joqe_ast_expr*
ast_expr_alloc()
{
  return calloc(1, sizeof(joqe_ast_expr));
}

static int
ast_expr_free(joqe_ast_expr *e)
{
  if(e) free(e);
  return 0;
}

static int
ast_unary_implode(joqe_ast_expr *e)
{
  if(e->u.e) {
    e->u.e->evaluate(e->u.e, IMPLODE);
    ast_expr_free(e->u.e);
  }
  return 0;
}
static int
ast_binary_implode(joqe_ast_expr *e)
{
  if(e->u.b.l) {
    e->u.b.l->evaluate(e->u.b.l, IMPLODE);
    ast_expr_free(e->u.b.l);
  }
  if(e->u.b.r) {
    e->u.b.r->evaluate(e->u.b.r, IMPLODE);
    ast_expr_free(e->u.b.r);
  }
  return 0;
}

static int
bool_eval_node(joqe_node rx, joqe_node *n)
{
  joqe_type v = JOQE_TYPE_VALUE(rx.type);
  switch(JOQE_TYPE_KEY(n->type)) {
    case joqe_type_string_none:
      return (v == joqe_type_none_string
          && 0 == strcmp(rx.u.s, n->k.key));
    case joqe_type_int_none:
      return (v == joqe_type_none_integer
          && rx.u.i == n->k.idx);
    // Design consideration, we do not treat a real-typed integer
    // value as an index. We could check for integer value, but that
    // would make the use inconsistent, better to always reject it, and
    // force the use of casting in that case.
    // TODO add type casting (-functions?)
    default:
      return v == joqe_type_none_true;
  }
}

static int
eval_fix_value(joqe_ast_expr *e, joqe_node *n, joqe_ctx *c, joqe_result *r)
{
  if(!n) return 0;
  if(n->type == joqe_type_ref_cnt)
    return 0; // never match ref count nodes.

  if(r) {
    joqe_type t = joqe_type_broken;
    switch(e->u.i) {
      case 1: t = joqe_type_none_true; break;
      case 0: t = joqe_type_none_false; break;
      case -1: t = joqe_type_none_null; break;
    }
    joqe_nodels *ls = result_nodels(r, t);
    ls->n.u.i = e->u.i;
  }
  return e->u.i > 0;
}

static int
eval_string_value(joqe_ast_expr *e, joqe_node *n, joqe_ctx *c, joqe_result *r)
{
  if(!n) return 0;

  if(r) {
    joqe_nodels *ls = result_nodels(r, joqe_type_none_string);
    ls->n.u.s = e->u.s;
    return !!*ls->n.u.s;
  } else {
    return JOQE_TYPE_KEY(n->type) == joqe_type_string_none
      && n->k.key && 0 == strcmp(n->k.key, e->u.s);
  }
}
static joqe_ast_expr
ast_string_value(const char *s)
{
  joqe_ast_expr e = {eval_string_value};
  e.u.s = s;
  return e;
}

static int
eval_stringls_value(joqe_ast_expr *e, joqe_node *n, joqe_ctx *ctx,
                    joqe_result *r)
{
  if(!n) {
    nodels_free(e->u.n.u.ls);
    return 0;
  }
  if(r) {
    joqe_nodels *ls = result_nodels(r, joqe_type_none_stringls);
    ls->n = joqe_result_copy_node(&e->u.n);
    return e->u.n.u.ls ? 1 : 0;
  } else {
    /* reversed strlsstrcmp arguments, so result should be negated,
       but it compares to 0 anyway. */
    return JOQE_TYPE_KEY(n->type) == joqe_type_string_none
      && n->k.key && 0 == strlsstrcmp(e->u.n, n->k.key);
  }
}

static joqe_ast_expr
ast_string_append(joqe_ast_expr e, const char *s)
{
  if(e.evaluate == eval_string_value) {
    joqe_ast_expr ne = {eval_stringls_value,
                        .u = {.n = {joqe_type_none_stringls}}};
    joqe_nodels prev = {.n = {joqe_type_none_string, .u = {.s = e.u.s}}},
               *prevls = malloc(sizeof(*prevls));
    *prevls = prev;

    joqe_list_append((joqe_list**)&ne.u.n.u.ls, &prevls->ll);
    e = ne;
  } else assert(e.evaluate == eval_stringls_value);

  joqe_nodels append = {.n = {joqe_type_none_string, .u = {.s = s}}},
             *appendls = malloc(sizeof(*appendls));
  *appendls = append;

  joqe_list_append((joqe_list**)&e.u.n.u.ls, &appendls->ll);
  return e;
}

static int
eval_integer_value(joqe_ast_expr *e, joqe_node *n, joqe_ctx *c, joqe_result *r)
{
  if(!n) return 0;

  if(r) {
    joqe_nodels *ls = result_nodels(r, joqe_type_none_integer);
    ls->n.u.i = e->u.i;
    return ls->n.u.i;
  } else {
    return JOQE_TYPE_KEY(n->type) == joqe_type_int_none && n->k.idx == e->u.i;
  }
}
static joqe_ast_expr
ast_integer_value(int i)
{
  joqe_ast_expr e = {eval_integer_value};
  e.u.i = i;
  return e;
}

static int
eval_real_value(joqe_ast_expr *e, joqe_node *n, joqe_ctx *c, joqe_result *r)
{
  if(!n) return 0;

  if(r) {
    joqe_nodels *ls = result_nodels(r, joqe_type_none_real);
    ls->n.u.d = e->u.d;
  }
  return 0;
}
static joqe_ast_expr
ast_real_value(double d)
{
  joqe_ast_expr e = {eval_real_value};
  e.u.d = d;
  return e;
}

// --expressions--

static joqe_ast_expr
ast_binary(joqe_ast_expr_eval eval, joqe_ast_expr l, joqe_ast_expr r)
{
  joqe_ast_expr *lp = ast_expr_alloc(),
                *rp = ast_expr_alloc(),
                e = {eval, .u = {.b = {0, lp, rp}}};
  *lp = l;
  *rp = r;

  return e;
}

static void
boolean_result(int val, joqe_result *r)
{
  joqe_nodels *o = joqe_result_alloc_node(r);
  o->n.type = val ? joqe_type_none_true : joqe_type_none_false;
  joqe_result_append(r, o);
}

static int
eval_bor (joqe_ast_expr *e,
          joqe_node *n, joqe_ctx *c,
          joqe_result *r)
{
  joqe_ast_expr *lp = e->u.b.l,
                *rp = e->u.b.r;

  if(!n) return ast_binary_implode(e);

  joqe_result ir = joqe_result_push(r),
             *irp = r ? &ir : 0;

  int rv = lp->evaluate(lp, n, c, irp);
  if(rv) {
    joqe_nodels *ls = ir.ls;
    ir.ls = 0;

    if(r)
      joqe_list_append((joqe_list**) &r->ls, (joqe_list*)ls);
  } else {
    rv = rp->evaluate(rp, n, c, r);
  }

  joqe_result_pop(r, &ir);

  return rv;
}

static joqe_ast_expr
ast_bor(joqe_ast_expr l, joqe_ast_expr r)
{
  return ast_binary(eval_bor, l, r);
}

static int
eval_band(joqe_ast_expr *e,
          joqe_node *n, joqe_ctx *c,
          joqe_result *r)
{
  joqe_ast_expr *lp = e->u.b.l,
                *rp = e->u.b.r;

  if(!n) return ast_binary_implode(e);

  int rv = lp->evaluate(lp, n, c, 0)
        && rp->evaluate(rp, n, c, r);


  return rv;
}

static joqe_ast_expr
ast_band(joqe_ast_expr l, joqe_ast_expr r)
{
  return ast_binary(eval_band, l, r);
}

static int
eval_compare(joqe_ast_expr *e, joqe_node *n, joqe_ctx *c, joqe_result *r)
{
  joqe_ast_comp_op op = (joqe_ast_comp_op)e->u.b.op;

  joqe_ast_expr *le = e->u.b.l,
                *re = e->u.b.r;
  joqe_result lr = joqe_result_push(r);
  int rv = 0;

  if(!n) return ast_binary_implode(e);

  le->evaluate(le, n, c, &lr);
  if(lr.ls) {
    joqe_result rr = joqe_result_push(&lr);
    re->evaluate(re, n, c, &rr);

    joqe_nodels *li, *ri;
    if((li = lr.ls)) do {
      if((ri = rr.ls)) do {
        int cmp, hit = 0;
        joqe_node *a = &li->n,
                  *b = &ri->n;

        joqe_type at = JOQE_TYPE_VALUE(a->type),
                  bt = JOQE_TYPE_VALUE(b->type);
        switch(at) {
          case joqe_type_none_true:
          case joqe_type_none_false:
          case joqe_type_none_null:
            if(op == joqe_ast_comp_eq)
              rv = (at == bt);
            else if (op == joqe_ast_comp_neq)
              rv = (at != bt);
            break;
          case joqe_type_none_string: switch(bt) {
            case joqe_type_none_string:
              cmp = strcmp(a->u.s, b->u.s);
              hit = 1;
              break;
            case joqe_type_none_stringls:
              cmp = -strlsstrcmp(*b, a->u.s);
              hit = 1;
              break;
            default:;
          } break;
          case joqe_type_none_stringls: switch(bt) {
            case joqe_type_none_string:
              cmp = strlsstrcmp(*a, b->u.s);
              hit = 1;
              break;
            case joqe_type_none_stringls:
              cmp = strlscmp(*a, *b);
              hit = 1;
              break;
            default:;
          } break;
          case joqe_type_none_integer: switch(bt) {
            case joqe_type_none_integer:
              cmp = a->u.i - b->u.i;
              hit = 1;
              break;
            case joqe_type_none_real: {
              double d = a->u.i - b->u.d;
              cmp = d < 0 ? -1 : d > 0 ? 1 : 0;
              hit = 1;
            } break;
            default:;
          } break;
          case joqe_type_none_real: switch(bt) {
            case joqe_type_none_integer: {
              double d = a->u.d - b->u.i;
              cmp = d < 0 ? -1 : d > 0 ? 1 : 0;
              hit = 1;
            } break;
            case joqe_type_none_real: {
              double d = a->u.d - b->u.d;
              cmp = d < 0 ? -1 : d > 0 ? 1 : 0;
              hit = 1;
            } break;
            default:;
          } break;
            //TODO compare objects, arrays?
          default:;
        }
        if(hit) {
          switch(op) {
            case joqe_ast_comp_eq: rv = !cmp; break;
            case joqe_ast_comp_neq: rv = !!cmp; break;
            case joqe_ast_comp_lt: rv = cmp<0; break;
            case joqe_ast_comp_lte: rv = cmp<=0; break;
            case joqe_ast_comp_gt: rv = cmp>0; break;
            case joqe_ast_comp_gte: rv = cmp>=0; break;
          }
        }

        if(rv) goto done;
      } while((ri = (joqe_nodels*)ri->ll.n) != rr.ls);
    } while((li = (joqe_nodels*)li->ll.n) != lr.ls);

    done:
    joqe_result_pop(&lr, &rr);
  }
  joqe_result_pop(r, &lr);

  if(r) boolean_result(rv, r);
  return rv;
}
static joqe_ast_expr
ast_bcompare(joqe_ast_comp_op op, joqe_ast_expr l, joqe_ast_expr r)
{
  joqe_ast_expr b = ast_binary(eval_compare, l, r);
  b.u.b.op = op;
  return b;
}


static joqe_node
calc_dbl(joqe_ast_calc_op op, double a, double b)
{
  joqe_node rx = {joqe_type_none_real};
  double d;

  switch(op) {
    case joqe_ast_calc_add: d = a + b; break;
    case joqe_ast_calc_sub: d = a - b; break;
    case joqe_ast_calc_mult: d = a * b; break;
    case joqe_ast_calc_div: d = a / b; break;
    case joqe_ast_calc_mod: d = fmod(a, b); break;
    default: d = 0;
  }

  rx.u.d = d;
  return rx;
}

static joqe_node
calc_int(joqe_ast_calc_op op, int a, int b)
{
  joqe_node rx = {joqe_type_none_integer};
  int i;

  switch(op) {
    case joqe_ast_calc_add: i = a + b; break;
    case joqe_ast_calc_sub: i = a - b; break;
    case joqe_ast_calc_mult: i = a * b; break;
    case joqe_ast_calc_div:
      if(b == 0) return calc_dbl(op, a, b); // div-by-zero -> double inf/nan
      i = a / b; break;
    case joqe_ast_calc_mod:
      if(b == 0) return calc_dbl(op, a, b); // div-by-zero -> double inf/nan
      i = a % b; break;
    default: i = 0;
  }

  rx.u.i = i;
  return rx;
}

static int
eval_calc(joqe_ast_expr *e, joqe_node *n, joqe_ctx *c, joqe_result *r)
{
  joqe_ast_calc_op op = (joqe_ast_calc_op)e->u.b.op;

  joqe_ast_expr *le = e->u.b.l,
                *re = e->u.b.r;
  joqe_result lr = joqe_result_push(r);
  int rv = 0;

  if(!n) return ast_binary_implode(e);

  le->evaluate(le, n, c, &lr);
  if(lr.ls) {
    joqe_result rr = joqe_result_push(&lr);
    re->evaluate(re, n, c, &rr);

    if(r) joqe_result_free_transfer(r, &lr);

    joqe_nodels *li, *ri;
    if((li = lr.ls)) do {
      if((ri = rr.ls)) do {
        joqe_node *a = &li->n,
                  *b = &ri->n;

        joqe_node rx = {};

        joqe_type at = JOQE_TYPE_VALUE(a->type),
                  bt = JOQE_TYPE_VALUE(b->type);
        switch(at) {
          case joqe_type_none_integer: switch(bt) {
            case joqe_type_none_integer:
              rx = calc_int(op, a->u.i, b->u.i);
              break;
            case joqe_type_none_real:
              rx = calc_dbl(op, a->u.i, b->u.d);
              break;
            default:
              continue;
          } break;
          case joqe_type_none_real: switch(bt) {
            case joqe_type_none_integer:
              rx = calc_dbl(op, a->u.d, b->u.i);
              break;
            case joqe_type_none_real:
              rx = calc_dbl(op, a->u.d, b->u.d);
              break;
            default:
              continue;
          } break;


          default: // can't do calculations using these.
            continue;
        }

        if(r) {
          rv++;
          joqe_nodels *o = joqe_result_alloc_node(r);
          o->n = rx;
          joqe_result_append(r, o);
        } else if(bool_eval_node(rx, n)) {
          rv = 1;
          goto done;
        }
      } while((ri = (joqe_nodels*)ri->ll.n) != rr.ls);
    } while((li = (joqe_nodels*)li->ll.n) != lr.ls);
    done:
    joqe_result_pop(&lr, &rr);
  }
  joqe_result_pop(r, &lr);
  return rv;
}

static joqe_ast_expr
ast_calc(joqe_ast_calc_op op, joqe_ast_expr l, joqe_ast_expr r)
{
  joqe_ast_expr b = ast_binary(eval_calc, l, r);
  b.u.b.op = op;
  return b;
}

static int
eval_posneg(int mul, joqe_ast_expr *e, joqe_node *n, joqe_ctx *c, joqe_result *r)
{
  joqe_ast_expr *pe = e->u.e;
  joqe_nodels *i;
  int rv = 0;

  if(!n) return ast_unary_implode(e);

  joqe_result er = joqe_result_push(r);

  pe->evaluate(pe, n, c, &er);

  if(r) joqe_result_free_transfer(r, &er);

  if((i = er.ls)) do {
    joqe_node *a = &i->n;
    joqe_node rx = *a;
    joqe_type t = JOQE_TYPE_VALUE(a->type);
    switch(t) {
      case joqe_type_none_integer: rx.u.i = mul * a->u.i; break;
      case joqe_type_none_real: rx.u.d = mul * a->u.d; break;
      default:
        continue;
    }

    if(r) {
      rv++;
      joqe_nodels *o = joqe_result_alloc_node(r);
      o->n = rx;
      joqe_result_append(r, o);
    } else if(bool_eval_node(rx, n)) {
      rv = 1;
      goto done;
    }
  } while((i = (joqe_nodels*)i->ll.n) != er.ls);
  done:
    ;
  joqe_result_pop(r, &er);
  return rv;
}

static int
eval_positive(joqe_ast_expr *e, joqe_node *n, joqe_ctx *c, joqe_result *r)
{
  return eval_posneg(1, e, n, c, r);
}

static int
eval_negative(joqe_ast_expr *e, joqe_node *n, joqe_ctx *c, joqe_result *r)
{
  return eval_posneg(-1, e, n, c, r);
}

static joqe_ast_expr
ast_negative(joqe_ast_expr e)
{
  // short circuit negative numbers
  if(e.evaluate == eval_integer_value) {
    e.u.i = -e.u.i;
    return e;
  } else if(e.evaluate == eval_real_value) {
    e.u.d = -e.u.d;
    return e;
  }
  joqe_ast_expr *ep = ast_expr_alloc(),
                ne = {eval_negative, .u = {.e = ep}};
  *ep = e;

  return ne;
}

static joqe_ast_expr
ast_positive(joqe_ast_expr e)
{
  // short circuit positive numbers
  if(e.evaluate == eval_integer_value || e.evaluate == eval_real_value)
    return e;

  joqe_ast_expr *ep = ast_expr_alloc(),
                ne = {eval_positive, .u = {.e = ep}};
  *ep = e;

  return ne;
}

static int
eval_not(joqe_ast_expr *e, joqe_node *n, joqe_ctx *c, joqe_result *r)
{

  joqe_ast_expr *ep = e->u.e;

  if(!n) return ast_unary_implode(e);

  int rv = !ep->evaluate(ep, n, c, 0);

  if(r) {
    joqe_nodels *o = joqe_result_alloc_node(r);
    o->n.type = rv ? joqe_type_none_true : joqe_type_none_false;
    joqe_result_append(r, o);
  }

  return rv;
}

static joqe_ast_expr
ast_bnot(joqe_ast_expr e)
{
  joqe_ast_expr *ep = ast_expr_alloc(),
                ne = {eval_not, .u = {.e = ep}};
  *ep = e;

  return ne;
}

static int
eval_context(joqe_ast_expr *e, joqe_node *n, joqe_ctx *c, joqe_result *r)
{
  joqe_nodels *i;
  joqe_result jr = joqe_result_push(r);
  int rv = 0;

  if(!n) {
    e->u.c.ctx.construct(&e->u.c.ctx, IMPLODE);
    e->u.c.e->evaluate(e->u.c.e, IMPLODE);
    return ast_expr_free(e->u.c.e);
  }

  e->u.c.ctx.construct(&e->u.c.ctx, n, c, &jr);

  if((i = jr.ls)) do {
    joqe_ctx stacked = {c, &i->n};
    rv += e->u.c.e->evaluate(e->u.c.e, &i->n, &stacked, r);
  } while((i = (joqe_nodels*)i->ll.n) != jr.ls);

  joqe_result_pop(r, &jr);
  return rv;
}

static joqe_ast_expr
ast_expr_context(joqe_ast_expr e, joqe_ast_construct ctx)
{
  joqe_ast_expr *ep = ast_expr_alloc(),
                ne = {eval_context, .u = {.c = {.e = ep, .ctx = ctx}}};
  *ep = e;

  return ne;
}

static int
eval_path(joqe_ast_expr *e, joqe_node *n, joqe_ctx *c, joqe_result *r)
{
    return e->u.path.visit(&e->u.path, n, c, r);
}

static joqe_ast_expr
ast_path_expr(joqe_ast_path p)
{
  joqe_ast_expr e = {eval_path};
  e.u.path = p;
  return e;
}

static joqe_ast_params
ast_params()
{
  joqe_ast_params p = {};
  return p;
}

static joqe_ast_params
ast_params_append(joqe_ast_params ps, joqe_ast_expr e)
{
  joqe_ast_paramls *ls = calloc(1, sizeof(*ls));
  ls->e = e;
  joqe_list_append((joqe_list**)&ps.ls, &ls->ll);
  ps.count++;
  return ps;
}


// --path--

static joqe_ast_path*
ast_path_alloc()
{
  return calloc(1, sizeof(joqe_ast_path));
}

static joqe_ast_pathelem*
ast_pathelem_alloc()
{
  return calloc(1, sizeof(joqe_ast_pathelem));
}

static int
ast_pathelem_free(joqe_ast_pathelem *p)
{
  if(p) free(p);
  return 0;
}

static int
ast_path_free(joqe_ast_path *p)
{
  if(p) free(p);
  return 0;
}

static joqe_ast_path
ast_union_path(joqe_ast_path l, joqe_ast_path r)
{
  l.punion = ast_path_alloc();
  *l.punion = r;
  return l;
}

static int
visit_local_path (joqe_ast_path *p,
                  joqe_node *n, joqe_ctx *c,
                  joqe_result *r)
{
  int v = 0;
  if(p->pes) {
    v = p->pes->visit(p->pes, n, c, r, p->pes);
    if(!n)
      ast_pathelem_free(p->pes);
  } else if(n) {
    if(r) result_nodels(r, n->type)->n = joqe_result_copy_node(n);
    v = 1; //bool_eval_node(*n, n); // heh..
  }
  if(p->punion) {
    // TODO union the result, not just concatenate.
    v += p->punion->visit(p->punion, n, c, r);
    if(!n)
      ast_path_free(p->punion);
  }

  return v;
}
static joqe_ast_path
ast_local_path()
{
  joqe_ast_path p = {visit_local_path};
  return p;
}

static int
visit_context_path (joqe_ast_path *p,
                    joqe_node *n, joqe_ctx *c,
                    joqe_result *r)
{
  int v = 0;
  int depth;
  joqe_ctx *cc = c;
  for(depth = p->i; depth > 0; --depth) {
    if(!cc) break;
    cc = cc->stack;
  }
  if(p->pes) {
    if(!n) {
      // imploding, c is null.
      v = p->pes->visit(p->pes, n, c, r, p->pes);
      ast_pathelem_free(p->pes);
    } else if(!cc) {
      v = 0;
    } else {
      v = p->pes->visit(p->pes, cc->node, c, r, p->pes);
    }
  } else if(n) {
    if(cc) {
      if(r) result_nodels(r, cc->node->type)->n = *cc->node;
      v = 1; //bool_eval_node(*cc, n);
    } else {
      v = 0;
    }
  }
  if(p->punion) {
    // TODO union the result, not just concatenate.
    v += p->punion->visit(p->punion, n, c, r);
    if(!n)
      ast_path_free(p->punion);
  }

  return v;
}
static joqe_ast_path
ast_context_path(int depth)
{
  joqe_ast_path p = {visit_context_path};
  p.i = depth;
  return p;
}

static joqe_ast_path
ast_path_chain(joqe_ast_path p, joqe_ast_pathelem pe)
{
  joqe_ast_pathelem *nxt = ast_pathelem_alloc();
  *nxt = pe;
  joqe_list_append((joqe_list**)&p.pes, &nxt->ll);
  return p;
}

static int
visit_pe_free (joqe_ast_pathelem *p, joqe_ast_pathelem *end)
{
  if(p->ll.n != &end->ll) {
    joqe_ast_pathelem *nxt = (joqe_ast_pathelem*) p->ll.n;
    nxt->visit(nxt, IMPLODE, end);
    return ast_pathelem_free(nxt);
  }
  return 0;
}

static int
call_concat (joqe_ast_pathelem *pe,
             joqe_node *n, joqe_ctx *c,
             joqe_nodels **ps, joqe_result *r)
{
  int pi, pcount = pe->u.func.ps.count;

  joqe_nodels *rn = result_nodels(r, joqe_type_none_stringls);
  for(pi = 0; pi < pcount; ++pi) {
    joqe_nodels *i;
    if((i = ps[pi])) do {
      if (JOQE_TYPE_VALUE(i->n.type) != joqe_type_none_string)
        continue;
      joqe_nodels *sn = joqe_result_alloc_node(r);
      sn->n = joqe_result_copy_node(&i->n);
      joqe_list_append((joqe_list**)&rn->n.u.ls, &sn->ll);
    } while((i = (joqe_nodels*)i->ll.n) != ps[pi]);
  }
  if(!rn->n.u.ls) {
    joqe_result_free_node(rn, r);
    return 0;
  }
  return 1;
}

static int
ast_params_destroy(joqe_ast_params *p)
{
  joqe_ast_paramls *i, *e, *ni;
  if ((e = i = p->ls)) do {
    i->e.evaluate(&i->e, IMPLODE);
    ni = (joqe_ast_paramls*)i->ll.n;
    free(i);
  } while((i = ni) != e);
  return 0;
}

static int
ast_params_evaluate(joqe_ast_params *p, joqe_node *n, joqe_ctx *c,
                    joqe_result *r, joqe_nodels **out)
{
  joqe_ast_paramls *i, *e, *ni;
  int outi = 0;
  if ((e = i = p->ls)) do {
    assert(outi < p->count);
    joqe_result rx = joqe_result_push(r);

    i->e.evaluate(&i->e, n, c, &rx);
    out[outi++] = rx.ls;
    rx.ls = 0;

    joqe_result_pop(r, &rx);
    ni = (joqe_ast_paramls*)i->ll.n;
  } while((i = ni) != e);
  return 0;
}

static int
visit_pefunction (joqe_ast_pathelem *p,
                  joqe_node *n, joqe_ctx *c,
                  joqe_result *r, joqe_ast_pathelem *end)
{
  joqe_nodels *i, *e;
  int pi, pcount = p->u.func.ps.count;
  int found = 0;

  if(!n) {
    ast_params_destroy(&p->u.func.ps);
    return visit_pe_free(p, end);
  }

  if(!p->u.func.call) {
    r->status |= joqe_result_fail;
    return 0;
  }

  joqe_nodels **params = alloca(sizeof(joqe_nodels*) * pcount);
  ast_params_evaluate(&p->u.func.ps, n, c, r, params);

  joqe_result fr = joqe_result_push(r);
  found = p->u.func.call(p, n, c, params, &fr);

  for(pi = 0; pi < pcount; ++pi) joqe_result_free_list(params[pi], r);

  if(p->ll.n != &end->ll) {
    found = 0;
    if((e = i = fr.ls)) do {
      joqe_ast_pathelem *nxt = (joqe_ast_pathelem*) p->ll.n;
      found += nxt->visit(nxt, &i->n, c, r, end);
    } while((!found || r) && (i = (joqe_nodels*)i->ll.n) != e);
  } else {
    if(r)
      joqe_result_transfer(r, &fr);
  }

  joqe_result_pop(r, &fr);
  return found;
}

static joqe_ast_pathelem
ast_pefunction(const char *name, joqe_ast_params p)
{
  struct {
    const char *name;
    joqe_function_call call;
  } funcs[] = {
    {"concat", call_concat},
    {0}
  }, *f;

  for(f = funcs; f->name && strcmp(f->name, name); ++f)
    ;

  joqe_ast_pathelem pefunction = {
    .visit = visit_pefunction,
    .u = {.func = {f->call, p}}
  };

  return pefunction;
}

static int
visit_peflex (joqe_ast_pathelem *p,
              joqe_node *n, joqe_ctx *c,
              joqe_result *r, joqe_ast_pathelem *end)
{
  joqe_nodels *i, *e;
  int found = 0;

  if(!n) return visit_pe_free(p, end);

  if(p->ll.n != &end->ll) {
    joqe_ast_pathelem *nxt = (joqe_ast_pathelem*) p->ll.n;
    found += nxt->visit(nxt, n, c, r, end);
  }

  joqe_type t = JOQE_TYPE_VALUE(n->type);
  if(t != joqe_type_none_object && t != joqe_type_none_array)
    return found;

  if((!found || r) && (e = i = n->u.ls)) do {
    found += visit_peflex(p, &i->n, c, r, end);
  } while((!found || r) && (i = (joqe_nodels*)i->ll.n) != e);

  return found;
}

static joqe_ast_pathelem
ast_peflex()
{
  joqe_ast_pathelem pename = {
    .visit = visit_peflex
  };
  return pename;
}

static int
visit_pename (joqe_ast_pathelem *p,
              joqe_node *n, joqe_ctx *c,
              joqe_result *r, joqe_ast_pathelem *end)
{
  joqe_nodels *i, *e;
  int found = 0;

  if(!n) return visit_pe_free(p, end);

  if(JOQE_TYPE_VALUE(n->type) != joqe_type_none_object)
    return 0;

  if((e = i = n->u.ls)) do {
    if (JOQE_TYPE_KEY(i->n.type) == joqe_type_string_none &&
        0 == strcmp(i->n.k.key, p->u.key))
    {
      if(p->ll.n != &end->ll) {
        joqe_ast_pathelem *nxt = (joqe_ast_pathelem*) p->ll.n;
        found += nxt->visit(nxt, &i->n, c, r, end);
      } else {
        if(r) result_nodels(r, c->node->type)->n = joqe_result_copy_node(&i->n);
        found = 1; //bool_eval_node(i->n, n); ?
      }
    }
  } while((!found || r) && (i = (joqe_nodels*)i->ll.n) != e);

  return found;
}

static joqe_ast_pathelem
ast_pename(const char *name)
{
  joqe_ast_pathelem pename = {
    .visit = visit_pename,
    .u = {.key = name}
  };
  return pename;
}

static int
visit_pefilter (joqe_ast_pathelem *p,
                joqe_node *n, joqe_ctx *c,
                joqe_result *r, joqe_ast_pathelem *end)
{
  joqe_nodels *i, *e;
  int found = 0;

  if(!n) {
    p->u.expr.evaluate(&p->u.expr, IMPLODE);
    return visit_pe_free(p, end);
  }

  joqe_type t = JOQE_TYPE_VALUE(n->type);
  if(t != joqe_type_none_object && t != joqe_type_none_array)
    return 0;

  if((e = i = n->u.ls)) do {
    if (p->u.expr.evaluate(&p->u.expr, &i->n, c, 0)) {
      if(p->ll.n != &end->ll) {
        joqe_ast_pathelem *nxt = (joqe_ast_pathelem*) p->ll.n;
        found += nxt->visit(nxt, &i->n, c, r, end);
      } else {
        if(r) result_nodels(r, c->node->type)->n = joqe_result_copy_node(&i->n);
        found = 1;//bool_eval_node(i->n, n); ?
      }
    }
  } while((!found || r) && (i = (joqe_nodels*)i->ll.n) != e);

  return found;
}

static joqe_ast_pathelem
ast_pefilter(joqe_ast_expr expr)
{
  joqe_ast_pathelem pename = {
    .visit = visit_pefilter,
    .u = {.expr = expr}
  };
  return pename;
}

// --construct--
static joqe_ast_objectls *
ast_objectls_alloc()
{
  return calloc(1, sizeof(joqe_ast_objectls));
}

static void
ast_objectls_free(joqe_ast_objectls *p)
{
  if(p) free(p);
}

static joqe_ast_arrayls *
ast_arrayls_alloc()
{
  return calloc(1, sizeof(joqe_ast_arrayls));
}

static int
ast_arrayls_free(joqe_ast_arrayls *p)
{
  if(p) free(p);
  return 0;
}

static int
construct_expr (joqe_ast_construct *cst, joqe_node *n, joqe_ctx *c,
                joqe_result *r)
{
  if(!n) {
    cst->u.expr->evaluate(cst->u.expr, 0, 0, 0);
    return ast_expr_free(cst->u.expr);
  }
  return cst->u.expr->evaluate(cst->u.expr, n, c, r);
}
static joqe_ast_construct
ast_expr_construct(joqe_ast_expr e)
{
  joqe_ast_construct c = {construct_expr};
  c.u.expr = ast_expr_alloc();
  *c.u.expr = e;
  return c;
}

static int
construct_object_entry (joqe_ast_construct *cst,
                        joqe_node *n, joqe_ctx *c,
                        joqe_result *r)
{
  joqe_ast_construct *kc = cst->u.ob.key,
                     *vc = cst->u.ob.value;

  if(!n) {
    kc->construct(kc, IMPLODE);
    vc->construct(vc, IMPLODE);
    ast_construct_free(kc);
    ast_construct_free(vc);
    return 0;
  }

  joqe_result kr = joqe_result_push(r),
              vr;

  kc->construct(kc, n, c, &kr);

  if(! kr.ls) {
    // TODO design consideration, currently: no key hit: skip entry
    joqe_result_pop(r, &kr);
    return 0;
  }

  joqe_nodels *k = (joqe_nodels*)
    joqe_list_detach((joqe_list**)&kr.ls, &kr.ls->ll);

  if(kr.ls) {
    //TODO design consideration, currently: multiple key hits: use first
  }
  joqe_result_pop(r, &kr);
  // free the key node into the r-results freelist from this point on
  // if we don't need it.

  if(JOQE_TYPE_VALUE(k->n.type) != joqe_type_none_string) {
    //TODO design consideration, currently: key is non-string: skip entry
    //Optionally: int/real -> render string?
    joqe_result_free_node(k, r);
    return 0;
  }
  vr = joqe_result_push(r);
  vc->construct(vc, n, c, &vr);
  if(! vr.ls) {
    //TODO design consideration, currently: no value: skip entry
    joqe_result_free_node(k, r);
    joqe_result_pop(r, &vr);
    return 0;
  } else {
    joqe_nodels *v = (joqe_nodels*)
      joqe_list_detach((joqe_list**)&vr.ls, &vr.ls->ll);
    if(vr.ls) {
      //TODO design consideration, currently: multiple value hits: use first
    }
    joqe_result_pop(r, &vr);

    v->n.type = JOQE_TYPE(joqe_type_string_none, v->n.type);
    v->n.k.key = k->n.u.s;
    joqe_result_free_node(k, r);
    joqe_result_append(r, v);
    return 1;
  }
}

static int
construct_object (joqe_ast_construct *cst,
                  joqe_node *n, joqe_ctx *c,
                  joqe_result *r)
{
  joqe_ast_objectls *i;
  if(!n) {
    joqe_ast_objectls *next, *end = cst->u.object.ls;
    if((i = end)) do {
      i->en.v.construct(&i->en.v, IMPLODE);
      next = (joqe_ast_objectls*)i->ll.n;
      ast_objectls_free(i);
    } while((i = next) != cst->u.object.ls);

    return 0;
  }

  // object construction in boolean context is always true.
  if(!r) return 1;

  joqe_nodels *o = result_nodels(r, joqe_type_none_object);
  joqe_result or = joqe_result_push(r);

  if((i = cst->u.object.ls)) do {
    i->en.v.construct(&i->en.v, n, c, &or);
  } while((i = (joqe_ast_objectls*)i->ll.n) != cst->u.object.ls);

  o->n.u.ls = or.ls;
  or.ls = 0;

  joqe_result_pop(r, &or);

  return 1;
}

static joqe_ast_construct
ast_object_construct(joqe_ast_object o)
{
  joqe_ast_construct c = {construct_object};
  c.u.object = o;
  return c;
}

static int
construct_array_in (joqe_ast_arentry *en,
                    joqe_node *n, joqe_ctx *c,
                    joqe_result *r)
{
  en->v.construct(&en->v, n, c, r);
  return 1;
}
static int
construct_array  (joqe_ast_construct *cst,
                  joqe_node *n, joqe_ctx *c,
                  joqe_result *r)
{
  joqe_ast_arrayls *i;

  if(!n) {
    joqe_ast_arrayls *next, *end = cst->u.array.ls;
    if((i = end)) do {
      construct_array_in(&i->en, IMPLODE);
      next = (joqe_ast_arrayls*)i->ll.n;
      ast_arrayls_free(i);
    } while((i = next) != cst->u.array.ls);

    return 0;
  }

  if(!r) return 1;

  joqe_nodels *o = result_nodels(r, joqe_type_none_array);
  joqe_result or = joqe_result_push(r);

  if((i = cst->u.array.ls)) do {
    construct_array_in(&i->en, n, c, &or);
  } while((i = (joqe_ast_arrayls*)i->ll.n) != cst->u.array.ls);

  o->n.u.ls = or.ls;
  or.ls = 0;
  joqe_result_pop(r, &or);

  int idx = 0;
  joqe_nodels *ni;
  if((ni = o->n.u.ls)) do {
    ni->n.type = JOQE_TYPE(joqe_type_int_none, ni->n.type);
    ni->n.k.idx = idx++;
  } while((ni = (joqe_nodels*)ni->ll.n) != o->n.u.ls);

  return 1;
}

static joqe_ast_construct
ast_array_construct(joqe_ast_array a)
{
  joqe_ast_construct c = {construct_array};
  c.u.array = a;
  return c;
}

static int
construct_context (joqe_ast_construct *cst,
                   joqe_node *n, joqe_ctx *c,
                   joqe_result *r)
{
  joqe_ast_construct *ctx = cst->u.ctx.context;
  joqe_ast_construct *v = cst->u.ctx.construction;
  int rc = 0;

  if(!n) {
    ctx->construct(ctx, IMPLODE);
    v->construct(v, IMPLODE);
    ast_construct_free(ctx);
    ast_construct_free(v);
    return 0;
  }

  joqe_result ctxr = joqe_result_push(r);
  joqe_nodels *ctxi;
  ctx->construct(ctx, n, c, &ctxr);
  joqe_result_free_transfer(r, &ctxr);

  if((ctxi = ctxr.ls)) do {
    joqe_ctx stacked = {c, &ctxi->n};
    rc += v->construct(v, &ctxi->n, &stacked, r);
  } while((ctxi = (joqe_nodels*)ctxi->ll.n) != ctxr.ls);

  return rc;
}

static joqe_ast_construct
ast_construct_context (joqe_ast_construct e, joqe_ast_construct c)
{
  joqe_ast_construct *ctx = ast_construct_alloc();
  joqe_ast_construct *cts = ast_construct_alloc();
  joqe_ast_construct r = {
    construct_context,
    {
      .ctx = {ctx, cts}
    }
  };
  *ctx = c;
  *cts = e;
  return r;
}

static joqe_ast_object
ast_object()
{
  joqe_ast_object o = {};
  return o;
}

static joqe_ast_object
ast_object_append(joqe_ast_object o, joqe_ast_construct en)
{
  joqe_ast_objectls *ls = ast_objectls_alloc();
  ls->en.v = en;
  joqe_list_append((joqe_list**)&o.ls, &ls->ll);
  return o;
}

static joqe_ast_array
ast_array()
{
  joqe_ast_array a = {};
  return a;
}

static joqe_ast_array
ast_array_append(joqe_ast_array a, joqe_ast_construct en)
{
  joqe_ast_arrayls *ls = ast_arrayls_alloc();
  ls->en.v = en;
  joqe_list_append((joqe_list**)&a.ls, &ls->ll);
  return a;
}

static joqe_ast_construct
ast_object_entry(joqe_ast_construct k, joqe_ast_construct v)
{
  joqe_ast_construct *key = ast_construct_alloc();
  joqe_ast_construct *value = ast_construct_alloc();
  joqe_ast_construct e = {
    construct_object_entry,
    {
      .ob = { key, value }
    }
  };

  *key = k;
  *value = v;
  return e;
}

static joqe_ast_construct
ast_object_copy_entry(joqe_ast_construct v)
{
  // this is a no-op, the expression selects key/name pairs.
  return v;
}

static joqe_ast_construct
ast_array_entry(joqe_ast_construct v)
{
  return v;
}

struct joqe_ast_api ast = {
  ast_string_value, // joqe_ast_expr (*string_value)(const char* s);
  ast_string_append, // joqe_ast_expr (*string_append)(joqe_ast_expr e, const char* s);
  ast_integer_value, // joqe_ast_expr (*integer_value)(int i);
  ast_real_value, // joqe_ast_expr (*real_value)(double d);

// --expressions--

  ast_bor,  // joqe_ast_expr (*bor)(joqe_ast_expr l, joqe_ast_expr r);
  ast_band, // joqe_ast_expr (*band)(joqe_ast_expr l, joqe_ast_expr r);
  ast_bcompare, // joqe_ast_expr (*bcompare)(joqe_ast_comp_op, joqe_ast_expr l, joqe_ast_expr r);
  ast_calc, // joqe_ast_expr (*calc)(joqe_ast_expr l, joqe_ast_expr r);
  ast_negative, // joqe_ast_expr (*negative)(joqe_ast_expr e);
  ast_positive, // joqe_ast_expr (*positive)(joqe_ast_expr e);
  ast_bnot, // joqe_ast_expr (*bnot)(joqe_ast_expr e);

  ast_expr_context, // joqe_ast_expr (*expr_context)(joqe_ast_expr e, joqe_ast_construct ctx);

  ast_path_expr, // joqe_ast_expr (*path_expression)(joqe_ast_path p);

  ast_params, // joqe_ast_params (*params)();
  ast_params_append, // joqe_ast_params (*params_append)(joqe_ast_params ps, joqe_ast_expr e);

// --path--

  ast_union_path, // joqe_ast_path (*punion)(joqe_ast_path l, joqe_ast_path r);
  ast_local_path, // joqe_ast_path (*local_path)();
  ast_context_path, // joqe_ast_path (*context_path)();
  ast_path_chain, // joqe_ast_path (*path_chain)(joqe_ast_path l, joqe_ast_pathelem pe);

  ast_pefunction, // joqe_ast_pathelem (*pefunction)(const char *name, joqe_ast_params p);
  ast_peflex, // joqe_ast_pathelem (*peflex)();
  ast_pename, // joqe_ast_pathelem (*pename)(const char *name);
  ast_pefilter, // joqe_ast_pathelem (*pefilter)(joqe_ast_expr filter);

// --construct--

  ast_expr_construct, // joqe_ast_construct (*expr_construct)(joqe_ast_expr e);
  ast_object_construct, // joqe_ast_construct (*object_construct)(joqe_ast_object o);
  ast_array_construct, // joqe_ast_construct (*array_construct)(joqe_ast_array a);
  ast_construct_context,  // joqe_ast_construct (*construct_context)(joqe_ast_construct e, joqe_ast_construct c);

  ast_object,       // joqe_ast_object (*object)();
  ast_object_append,// joqe_ast_object (*object_append)(joqe_ast_object o, joqe_ast_obentry e);
  ast_array,        // joqe_ast_array  (*array)();
  ast_array_append, // joqe_ast_array  (*array_append)(joqe_ast_array a, joqe_ast_arentry e);

  ast_object_entry, // joqe_ast_entry   (*object_entry)(joqe_ast_expr k, joqe_ast_construct v);
  ast_object_copy_entry, // joqe_ast_entry   (*object_copy_entry)(joqe_ast_expr v);
  ast_array_entry, // joqe_ast_construct   (*array_entry)(joqe_ast_construct v);

  {eval_fix_value, .u = {.i = 1}}, // joqe_ast_expr true_value;
  {eval_fix_value, .u = {.i = 0}}, // joqe_ast_expr false_value;
  {eval_fix_value, .u = {.i = -1}} // joqe_ast_expr null_value;
};
