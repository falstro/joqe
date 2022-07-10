%token IDENTIFIER
%token STRING PARTIALSTRING INTEGER REAL
%token _TRUE _FALSE _NULL
%token MOD
%token NEQ LEQ GEQ
%token AND OR NOT
%token DOT2 SLASH C2
%token INVALID_STRING

%type<string> STRING PARTIALSTRING IDENTIFIER name
%type<integer> INTEGER INVALID_STRING SLASH
%type<real> REAL

%type<expr> expr scalar or-expr and-expr test-expr term-expr
            term factor unary string part-string
%type<construct> object-entry
%type<construct> array-entry
%type<pe> function filter
%type<construct> construct-expr simple-construct object-construct array-construct
%type<integer> compare-op
%type<path> node-set node-path
%type<params> params
%type<object> object-entries
%type<array> array-entries

%define parse.error verbose
%define api.pure true
%define api.prefix {joqe_yy}
%defines

%{
#include <stdint.h>
#include "ast.h"
#include "lex-source.h"
#include "build.h"
%}

%union {
  int64_t         integer;
  const char     *string;
  double          real;
  joqe_ast_expr   expr;
  joqe_ast_path   path;
  joqe_ast_params params;

  joqe_ast_pathelem   pe;

  joqe_ast_construct  construct;
  joqe_ast_object     object;
  joqe_ast_array      array;
}

%{
#include "lex.h"
#include "err.h"
%}

%lex-param { joqe_build *build }
%parse-param { joqe_build *build }

%%

start       : construct-expr
{
  build->root = $1;
}
            | error INVALID_STRING
{
  joqe_yyerror(build, joqe_invalid_string($2));
  return -1;
}
            ;

/* path/selection expressions */

expr        : or-expr
            ;
scalar      : string
            | INTEGER                           {$$ = ast.integer_value($1);}
            | REAL                              {$$ = ast.real_value($1);}
            | _TRUE                             {$$ = ast.true_value;}
            | _FALSE                            {$$ = ast.false_value;}
            | _NULL                             {$$ = ast.null_value;}
            ;

string      : STRING                            {$$ = ast.string_value($1);}
            | string STRING                     {$$ = ast.string_append($1, $2);}
            | part-string STRING                {$$ = ast.string_append($1, $2);}
            ;
part-string : PARTIALSTRING                     {$$ = ast.string_value($1);}
            | part-string PARTIALSTRING         {$$ = ast.string_append($1, $2);}
            ;

or-expr     : and-expr
            | or-expr OR and-expr               {$$ = ast.bor($1,$3);}
            ;
and-expr    : test-expr
            | and-expr AND test-expr            {$$ = ast.band($1,$3);}
            ;
test-expr   : term-expr
            | term-expr compare-op term-expr    {$$ = ast.bcompare($2,$1,$3);}
            ;
term-expr   : term
            | term-expr '+' term                {$$ = ast.calc(joqe_ast_calc_add,$1,$3);}
            | term-expr '-' term                {$$ = ast.calc(joqe_ast_calc_sub,$1,$3);}
            ;
term        : factor
            | term '*' factor                   {$$ = ast.calc(joqe_ast_calc_mult,$1,$3);}
            | term '/' factor                   {$$ = ast.calc(joqe_ast_calc_div,$1,$3);}
            | term '%' factor                   {$$ = ast.calc(joqe_ast_calc_mod,$1,$3);}
            | term MOD factor                   {$$ = ast.calc(joqe_ast_calc_mod,$1,$3);}
            ;
factor      : unary
            | '-' unary                         {$$ = ast.negative($2);}
            | '+' unary                         {$$ = ast.positive($2);}
            | NOT unary                         {$$ = ast.bnot($2);}
            ;
unary       : scalar
            | node-set                          {$$ = ast.path_expression($1);}
          /*| function */
            | '(' expr ')'                      {$$ = $2;}
            | '(' construct-expr C2 expr ')'    {$$ = ast.expr_context($4,$2);}
            ;
compare-op  : '='                               {$$ = joqe_ast_comp_eq;}
            | NEQ                               {$$ = joqe_ast_comp_neq;}
            | '<'                               {$$ = joqe_ast_comp_lt;}
            | LEQ                               {$$ = joqe_ast_comp_lte;}
            | '>'                               {$$ = joqe_ast_comp_gt;}
            | GEQ                               {$$ = joqe_ast_comp_gte;}
            ;
node-set    : node-path
            | node-set '|' node-path            {$$ = ast.punion($1,$3);}
            ;
node-path   : '.'                               {$$ = ast.local_path();}
            | '/'                               {$$ = ast.context_path(0);}
            | SLASH                             {$$ = ast.context_path($1);}
            | name                              {
                                                  $$ = ast.path_chain(
                                                    ast.local_path(),
                                                    ast.pename($1));
                                                }
            | '.' name                          {
                                                  $$ = ast.path_chain(
                                                    ast.local_path(),
                                                    ast.pename($2));
                                                }
            | '/' name                          {
                                                  $$ = ast.path_chain(
                                                    ast.context_path(0),
                                                    ast.pename($2));
                                                }
            | SLASH name                        {
                                                  $$ = ast.path_chain(
                                                    ast.context_path($1),
                                                    ast.pename($2));
                                                }
            | DOT2 name                         {
                                                  $$ = ast.path_chain(
                                                    ast.path_chain(
                                                      ast.local_path(),
                                                      ast.peflex()),
                                                    ast.pename($2));
                                                }
            | DOT2 '[' filter ']'               {
                                                  $$ = ast.path_chain(
                                                    ast.path_chain(
                                                      ast.local_path(),
                                                      ast.peflex()),
                                                    $3);
                                                }
            | node-path '.' name                { $$ = ast.path_chain($1,
                                                    ast.pename($3));}
            | node-path '[' filter ']'          { $$ = ast.path_chain($1,$3); }
            | node-path DOT2 name               { $$ = ast.path_chain(
                                                    ast.path_chain(
                                                      $1, ast.peflex()
                                                    ),
                                                    ast.pename($3));
                                                }
            | node-path DOT2 '[' filter ']'     { $$ = ast.path_chain(
                                                    ast.path_chain(
                                                      $1, ast.peflex()
                                                    ), $4);
                                                }
            | function                          {
                                                  $$ = ast.path_chain(
                                                    ast.local_path(), $1);
                                                }
            | node-path '.' function            { $$ = ast.path_chain($1,$3); }
            | node-path DOT2 function           {
                                                  $$ = ast.path_chain(
                                                    ast.path_chain($1,
                                                      ast.peflex()),
                                                    $3);
                                                }
            ;
name        : IDENTIFIER
            ;
filter      : /* empty */                       {$$ = ast.pefilter(ast.true_value);}
            | expr                              {$$ = ast.pefilter($1);}
            ;
function    : name '(' params ')'               {$$ = ast.pefunction($1,$3);}
            | name '(' ')'                      {$$ = ast.pefunction($1,
                                                  ast.params());}
            ;
params      : expr                              {
                                                  $$ = ast.params_append(
                                                    ast.params(), $1);
                                                }
            | params ',' expr                   {
                                                  $$ = ast.params_append($1,$3);
                                                }
            ;


/* construct expressions */

simple-construct:  object-construct
               |  array-construct
               |  expr                          {$$ = ast.expr_construct($1);}
               ;
construct-expr :  simple-construct
               |  construct-expr C2 simple-construct
                                                {$$ = ast.construct_context($3,$1);}
               ;

object-construct: '{' object-entries '}'        {$$ = ast.object_construct($2);}
               |  '{' object-entries ',' '}'    {$$ = ast.object_construct($2);}
               |  '{' '}'                       {
                                                  $$ = ast.object_construct(
                                                    ast.object());
                                                }
               ;
object-entries :  object-entry                  {
                                                  $$ = ast.object_append(
                                                    ast.object(),$1
                                                  );
                                                }
               |  object-entries ',' object-entry
                                                {
                                                  $$ = ast.object_append($1,$3);
                                                }
               ;
object-entry   :  simple-construct ':' construct-expr
                                                {$$ = ast.object_entry($1,$3);}
               |  simple-construct              {$$ = ast.object_copy_entry($1);}
               |  simple-construct C2 object-entry
                                                {$$ = ast.construct_context($3,$1);}
               ;
array-construct:  '[' array-entries ']'         {$$ = ast.array_construct($2);}
               |  '[' array-entries ',' ']'     {$$ = ast.array_construct($2);}
               |  '[' ']'                       {
                                                  $$ = ast.array_construct(
                                                    ast.array()
                                                  );
                                                }
               ;
array-entries  :  array-entry                   {
                                                  $$ = ast.array_append(
                                                    ast.array(), $1
                                                  );
                                                }
               |  array-entries ',' array-entry {
                                                  $$ = ast.array_append($1,$3);
                                                }
               ;
array-entry    :  construct-expr                {$$ = ast.array_entry($1);}
               ;
