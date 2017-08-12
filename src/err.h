#ifndef __JOQE_ERR_H__
#define __JOQE_ERR_H__


struct joqe_build;
int joqe_yyerror (struct joqe_build  *build,  // may be null
                  const char         *msg);
const char *joqe_invalid_string(int   code);

#endif /* idempotent include guard */
