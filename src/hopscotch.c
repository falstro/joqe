#include "hopscotch.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef TRACE
#include <stdio.h>
#define ENTER D("%s enter", __func__)
#define ENTERI(x) D("%s enter: %d", __func__, x)
#define LEAVE D("%s leave", __func__)
#define LEAVEI(x) D("%s leave: %d", __func__, x)
#else
#define ENTER
#define ENTERI(...)
#define LEAVE
#define LEAVEI(...)
#endif

#ifdef DEBUG
#include <stdio.h>
#define D(...) do { fprintf(stderr,  __VA_ARGS__); fprintf(stderr, "\n"); } while(0);
#else
#define D(...)
#endif

#define SLOTS 8
#define FULLBUCKET ((1<<SLOTS)-1)
#define ID(idm) ((idm) >> SLOTS)
#define MASK(idm) ((idm) & FULLBUCKET)

typedef struct {
  int   id_mask;
  int   hash;
  void *slot;
} bucket;

struct hopscotch_s {
  int     size;
  int     inuse;
  bucket  buckets[];
};

hopscotch
hopscotch_create()
{
  ENTER;LEAVE;
  return (void*)0;
}

void
hopscotch_destroy(hopscotch *table)
{
  ENTER;
  if(table) {
    if(*table)
      free(*table);
    *table = 0;
  }
  LEAVE;
}

#define tablesize(buckets) \
  (sizeof(struct hopscotch_s) + sizeof(bucket[buckets]))

static hopscotch
expand_table (hopscotch table, int add)
{
  ENTER;
  hopscotch ntable = realloc(table, tablesize(table->size+add));
  memset(&ntable->buckets[ntable->size], 0, sizeof(bucket[add]));
  ntable->size += add;
  LEAVE;
  return ntable;
}

static void
rehash_half_table (hopscotch t)
{
  int i, h, hmask;
  ENTER;
  h = t->size >> 1;
  hmask = h-1;
  for(i = 0; i < h; ++i)
  {
    int id = ID(t->buckets[i].id_mask);
    if(t->buckets[i].hash & h) {
      int s = 1<<id;
      // don't need to keep the mask at i+h, there's no mask there.
      t->buckets[i+h].id_mask = id << SLOTS;

      assert(t->buckets[(i+h-id)&hmask].id_mask & s);

      t->buckets[(i+h-id)&hmask].id_mask ^= s;
      t->buckets[(i+h-id)].id_mask |= s;
      t->buckets[i+h].hash = t->buckets[i].hash;
      t->buckets[i+h].slot = t->buckets[i].slot;
      t->buckets[i].hash = 0;
      t->buckets[i].slot = 0;
      t->buckets[i].id_mask = MASK(t->buckets[i].id_mask);
    }
  }
  for(i = 0; i < SLOTS; ++i) {
    int id = ID(t->buckets[i].id_mask);
    if(id > i) {
      int s = 1<<id;
      // these wrap, but don't move, move masks to the new end...
      t->buckets[(i+h-id)&hmask].id_mask ^= s;
      t->buckets[(i+t->size-id)].id_mask |= s;
    }
  }

  LEAVE;
}

#define CTZ(x) __builtin_ctz(x)

static void
bucket_insert  (hopscotch  *table,
                int         hash,
                void       *object)
{
  hopscotch t = *table;
  int i, s, mask, base, end;
  bucket *bs, *b;

  tailrecurse:
  mask = t->size - 1;
  base = hash&mask;
  end = base + t->size;
  bs = t->buckets;
  b = &bs[base];

  D("looking to insert 0x%x at %d", hash, base);

  for(i = base; i < end; ++i) {
    int ix = i&mask;
    // we can't hop past a full bucket, might as well rehash immediately
    if(MASK(bs[ix].id_mask) == FULLBUCKET)
      break;
    else if(!bs[ix].slot) {
      D("first free slot is %d(%x)", i, ix);
      int j, id = i - base;
      while(id >= SLOTS) {
        for(j = i-(SLOTS-1), s = 1<<(SLOTS-1); j < i; ++j, s>>=1) {
          int jx = j&mask;
          int pop = MASK(bs[jx].id_mask);
          if(pop) {
            int lbit = CTZ(pop);
            int jj = j+lbit;
            int jjx = jj&mask;
            if (jj < i) {
              s |= 1<<lbit;
              D("hop %d/%d -> %d", j, jj, i);
              bs[ix].hash = bs[jjx].hash;
              bs[ix].slot = bs[jjx].slot;
              bs[ix].id_mask = MASK(bs[ix].id_mask) | (i-j) << SLOTS;
              bs[jx].id_mask ^= s;
              bs[jjx].hash = 0;
              bs[jjx].slot = 0;
              bs[jjx].id_mask = MASK(bs[jjx].id_mask);
              id = jj-base;
              ix = jjx;
              i = jj;
              goto hop;
            }
          }
        }
        goto rehash;
        hop: ;
      }

      b->id_mask |= 1<<id;
      bs[ix].hash = hash;
      bs[ix].slot = object;
      bs[ix].id_mask |= id << SLOTS;
      D("inserted @ %d/%d", base, ix);
      t->inuse++;
      return;
    }
  }

  rehash:
  if(t->size > t->inuse<<4) {
    D("aborting: %d/%d", t->inuse, t->size);
    // load factor less than 16:1, hash function apparently unusable
    abort(); // TODO don't abort()
  }

  // no eligible slots; rehash.
#ifdef DEBUG
  D("expanding table: %d", 2*t->size);
  int bi, pc;
  for(bi = 0, pc = 0; bi < t->size; ++bi) {
    int px = __builtin_popcount(MASK(t->buckets[bi].id_mask));
    fprintf(stderr, "%x(%d)", px, ID(t->buckets[bi].id_mask));
    pc += px;
  }
  fprintf(stderr, "\npop: %d inuse: %d size: %d\n", pc, t->inuse, t->size);
#endif
  *table = t = expand_table(t, t->size);
  rehash_half_table(t);
  goto tailrecurse;
}

static int
bucket_fetch (hopscotch  *table,
              int         hash)
{
  hopscotch t = *table;
  int i, s, mask = t->size - 1, base = hash&mask;
  bucket *bs = t->buckets;
  bucket *b = &bs[base];

  ENTERI(base);

  if(MASK(b->id_mask)) {
    for(i = 0, s = 1; i < SLOTS; ++i, s<<=1) {
      if(b->id_mask & s) {
        int ix = (base+i)&mask;
        if (bs[ix].hash == hash && bs[ix].slot) {
          LEAVEI(ix);
          return ix;
        }
      }
    }
  }

  LEAVE;
  return -1;
}

void*
hopscotch_insert (hopscotch  *table,
                  int         hash,
                  void       *object)
{
  hopscotch t = *table;
  ENTER;

  if (!t) {
    *table = t = malloc(tablesize(SLOTS));
    t->size = SLOTS;
    t->inuse = 0;
    memset(t->buckets, 0, sizeof(bucket[t->size]));
  } else if(t->size == 0) {
    *table = t = expand_table(t, SLOTS);
  }

  int ix = bucket_fetch(table, hash);
  if(ix >= 0) {
    void *old = t->buckets[ix].slot;
    t->buckets[ix].slot = object;
    LEAVE;
    return old;
  }

  bucket_insert(table, hash, object);
  LEAVE;
  return (void*)0;
}

void*
hopscotch_fetch(hopscotch  *table,
                int         hash)
{
  hopscotch t = *table;
  ENTER;
  if (!t || !t->size) {
    return (void*)0;
  }

  int ix = bucket_fetch(table, hash);
  LEAVE;
  if(ix >= 0)
    return t->buckets[ix].slot;
  return (void*)0;
}

void*
hopscotch_remove (hopscotch  *table,
                  int         hash)
{
  hopscotch t = *table;
  ENTER;
  if (t && t->size) {
    int ix = bucket_fetch(table, hash);
    if(ix >= 0) {
      int mask = t->size - 1;
      void *object = t->buckets[ix].slot;
      int id = ID(t->buckets[ix].id_mask);
      bucket *b = &t->buckets[(ix+t->size-id)&mask];
      b->id_mask ^= 1<<id;
      t->buckets[ix].slot = 0;
      t->inuse--;
      LEAVE;
      return object;
    }
  }
  LEAVE;
  return (void*)0;
}
