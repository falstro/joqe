#ifndef __JOQE_HOPSCOTCH_H__
#define __JOQE_HOPSCOTCH_H__

typedef struct hopscotch_s *hopscotch;

hopscotch   hopscotch_create ();
void*       hopscotch_insert (hopscotch  *table,
                              int         hash,
                              void       *object);
void*       hopscotch_fetch  (hopscotch  *table,
                              int         hash);
void*       hopscotch_remove (hopscotch  *table,
                              int         hash);
void        hopscotch_destroy(hopscotch  *table);

#endif /* idempotent include guard */
