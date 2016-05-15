#ifndef __JOQE_UTIL_H__
#define __JOQE_UTIL_H__

typedef struct joqe_list {
  struct joqe_list *n,*p;
} joqe_list;

void        joqe_list_append(joqe_list **ls, joqe_list *e);
joqe_list*  joqe_list_detach(joqe_list **ls, joqe_list *e);

#endif /* idempotent include guard */
