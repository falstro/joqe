#ifndef __JOQE_AST_H__
#define __JOQE_AST_H__

#include "json.h"

#include <stdint.h>

typedef struct joqe_result {
  joqe_nodels *ls;
  joqe_nodels *freels;
} joqe_result;

joqe_nodels*  joqe_result_alloc_node (joqe_result *r);
joqe_node     joqe_result_copy_node  (joqe_node   *n);
void          joqe_result_clear_node (joqe_node    n,
                                      joqe_result *r);
void          joqe_result_free_node  (joqe_nodels *n,
                                      joqe_result *r);
void          joqe_result_destroy    (joqe_result *r);

typedef struct joqe_ast_objectls joqe_ast_objectls;
typedef struct joqe_ast_arrayls joqe_ast_arrayls;
typedef struct joqe_ast_expr joqe_ast_expr;

typedef struct {
  joqe_ast_objectls *ls;
} joqe_ast_object;
typedef struct {
  joqe_ast_arrayls *ls;
} joqe_ast_array;

typedef struct joqe_ctx {
  struct joqe_ctx    *stack;
  joqe_node          *node;
} joqe_ctx;

typedef struct joqe_ast_construct joqe_ast_construct;
struct joqe_ast_construct {
  int (*construct) (joqe_ast_construct *cst,
                    joqe_node *n, joqe_ctx *c,
                    joqe_result *r);
  union {
    joqe_node         node;
    joqe_ast_object   object;
    joqe_ast_array    array;
    joqe_ast_expr    *expr;

    struct {
      joqe_ast_construct *key;
      joqe_ast_construct *value;
    } ob;

    struct {
      joqe_ast_construct *context;
      joqe_ast_construct *construction;
    } ctx;
  } u;
};

typedef struct joqe_ast_pathelem joqe_ast_pathelem;

typedef struct joqe_ast_path {
  int (*visit) (struct joqe_ast_path *p,
                joqe_node *n, joqe_ctx *c,
                joqe_result *r);

  joqe_ast_pathelem    *pes;
  struct joqe_ast_path *punion;
  int                   i;
} joqe_ast_path;

typedef
int (*joqe_ast_expr_eval)(struct joqe_ast_expr *e,
                          joqe_node *n, joqe_ctx *c,
                          joqe_result *r);
typedef struct joqe_ast_expr {
  joqe_ast_expr_eval evaluate;
  union {
    const char     *s;
    int64_t         i;
    double          d;
    joqe_ast_path   path;
    joqe_ast_expr  *e;
    struct {
      joqe_ast_expr      *e;
      joqe_ast_construct  ctx;
    } c;
    struct {
      int             op;
      joqe_ast_expr  *l, *r;
    } b;
  } u;
} joqe_ast_expr;

struct joqe_ast_pathelem {
  joqe_list     ll;
  int (*visit) (struct joqe_ast_pathelem *p,
                joqe_node *n, joqe_ctx *c,
                joqe_result *r,
                struct joqe_ast_pathelem *end);
  union {
    const char     *key;
    int             idx;
    joqe_ast_expr   expr;
  } u;
};

typedef struct {
  joqe_list ll;
  joqe_ast_expr e;
} joqe_ast_paramls;

typedef struct {
  joqe_ast_paramls *ls;
} joqe_ast_params;

typedef struct joqe_ast_obentry {
  joqe_ast_construct v;
} joqe_ast_obentry;
struct joqe_ast_objectls {
  joqe_list ll;
  joqe_ast_obentry en;
};

typedef struct joqe_ast_arentry {
  joqe_ast_construct v;
} joqe_ast_arentry;
struct joqe_ast_arrayls {
  joqe_list ll;
  joqe_ast_arentry en;
};

typedef enum {
  joqe_ast_comp_eq,
  joqe_ast_comp_neq,
  joqe_ast_comp_lt,
  joqe_ast_comp_lte,
  joqe_ast_comp_gt,
  joqe_ast_comp_gte
} joqe_ast_comp_op;

typedef enum {
  joqe_ast_calc_add,
  joqe_ast_calc_sub,
  joqe_ast_calc_mult,
  joqe_ast_calc_div,
  joqe_ast_calc_mod,
} joqe_ast_calc_op;

extern struct joqe_ast_api {
  joqe_ast_expr (*string_value)(const char* s);
  joqe_ast_expr (*integer_value)(int i);
  joqe_ast_expr (*real_value)(double d);

  joqe_ast_expr (*bor)(joqe_ast_expr l, joqe_ast_expr r);
  joqe_ast_expr (*band)(joqe_ast_expr l, joqe_ast_expr r);
  joqe_ast_expr (*bcompare)(joqe_ast_comp_op, joqe_ast_expr l, joqe_ast_expr r);
  joqe_ast_expr (*calc)(joqe_ast_calc_op, joqe_ast_expr l, joqe_ast_expr r);
  joqe_ast_expr (*negative)(joqe_ast_expr e);
  joqe_ast_expr (*positive)(joqe_ast_expr e);
  joqe_ast_expr (*bnot)(joqe_ast_expr e);

  joqe_ast_expr (*expr_context)(joqe_ast_expr e, joqe_ast_construct ctx);

  joqe_ast_expr (*path_expression)(joqe_ast_path p);

  joqe_ast_params (*params)();
  joqe_ast_params (*params_append)(joqe_ast_params ps, joqe_ast_expr e);

  joqe_ast_path (*punion)(joqe_ast_path l, joqe_ast_path r);
  joqe_ast_path (*local_path)();
  joqe_ast_path (*context_path)(int depth);
  joqe_ast_path (*path_chain)(joqe_ast_path l, joqe_ast_pathelem pe);

  joqe_ast_pathelem   (*pefunction)(const char *name, joqe_ast_params p);
  joqe_ast_pathelem   (*peflex)();
  joqe_ast_pathelem   (*pename)(const char *name);
  joqe_ast_pathelem   (*pefilter)(joqe_ast_expr filter);

  joqe_ast_construct  (*expr_construct)(joqe_ast_expr e);
  joqe_ast_construct  (*object_construct)(joqe_ast_object o);
  joqe_ast_construct  (*array_construct)(joqe_ast_array a);
  joqe_ast_construct  (*construct_context)(joqe_ast_construct e, joqe_ast_construct c);

  joqe_ast_object     (*object)();
  joqe_ast_object     (*object_append)(joqe_ast_object o, joqe_ast_construct e);
  joqe_ast_array      (*array)();
  joqe_ast_array      (*array_append)(joqe_ast_array a, joqe_ast_construct e);

  joqe_ast_construct  (*object_entry)(joqe_ast_construct k, joqe_ast_construct v);
  joqe_ast_construct  (*object_copy_entry)(joqe_ast_construct v);
  joqe_ast_construct  (*array_entry)(joqe_ast_construct v);

  joqe_ast_expr     true_value;
  joqe_ast_expr     false_value;
  joqe_ast_expr     null_value;
} ast;

#endif /* idempotent include guard */
