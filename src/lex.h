#ifndef __JOQE_LEX_H__
#define __JOQE_LEX_H__

#define INVALID_END_OF_INPUT     -1
#define INVALID_UNKNOWN           0
#define INVALID_OVERLONG          1
#define INVALID_CONTROL_CHARACTER 2
#define INVALID_UNICODE_ESCAPE    3
#define INVALID_UNICODE_SURROGATE 4

union JOQE_YYSTYPE;
struct joqe_build;
int joqe_yylex (union JOQE_YYSTYPE *yylval,
                struct joqe_build  *builder);

#endif /* idempotent include guard */
