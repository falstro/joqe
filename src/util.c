#include "util.h"

void
joqe_list_append(joqe_list **ls, joqe_list *e)
{
  if(!e)
    return;
  if(!e->n || !e->p)
    e->n = e->p = e;

  if(!*ls) {
    *ls = e;
  } else {
    joqe_list *first, *last;

    first = e;
    last = e->p;

    joqe_list *r = *ls;
    first->p = r->p;
    last->n = r;
    r->p->n = first;
    r->p = last;
  }
}

joqe_list*
joqe_list_detach(joqe_list **ls, joqe_list *e)
{
  if(e == *ls) {
    if (e->n == e)
      *ls = 0;
    else
      *ls = e->n;
  }

  e->n->p = e->p;
  e->p->n = e->n;
  e->n = e->p = e;
  return e;
}
