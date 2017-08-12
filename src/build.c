#include "ast.h"
#include "lex-source.h"
#include "build.h"

#include <stdlib.h>

#include <string.h>

#define SLABSZ 0x800000
#define STRINGSZ (SLABSZ-sizeof(struct joqe_slab))

#define MARKERMASK  0xFF000000u
#define MARKERLAST  0x01000000u
#define MARKERFIRST 0x01u

#define MURMURSEED  0x13c4d906u

#ifdef DEBUG
#include <stdio.h>
#define D(...) do {fprintf(stderr, __VA_ARGS__); fprintf(stderr,"\n"); } while(0)
#else
#define D(...) do {} while(0)
#endif

static joqe_slab*
joqe_slab_create ()
{
  joqe_slab *s = malloc(SLABSZ);
  s->nxt = 0;
  s->mark = s->write = 0;
  //D("allocating slab\n");
  return s;
}

static void
joqe_slab_free (joqe_slab*s)
{
  //D("freeing slab\n");
  free(s);
}

static inline void
joqe_build_reset_hash(joqe_build* build)
{
  build->block = MARKERFIRST;
  build->hash = MURMURSEED;
}

joqe_build
joqe_build_init(joqe_lex_source src)
{
  joqe_build b = {src};
  b.interned = hopscotch_create();
  joqe_build_reset_hash(&b);
  return b;
}
void
joqe_build_destroy(joqe_build *b)
{
  hopscotch_destroy(&b->interned);
  for(joqe_slab *c, *s = b->first; (c=s); s=s->nxt, joqe_slab_free(c))
    ;
  b->first = b->current = 0;

  if(b->root.construct) {
    b->root.construct(&b->root, 0, 0, 0);
    b->root.construct = 0;
  }
}

#define ROTL32(x,r) (((x) << (r)) | ((x) >> (32 - (r))))
static uint32_t
murmur3_block(uint32_t h1, uint32_t k1)
{
  static const uint32_t c1 = 0xcc9e2d51;
  static const uint32_t c2 = 0x1b873593;
  static const uint32_t c3 = 0xe6546b64;

  k1 *= c1;
  k1 = ROTL32(k1,15);
  k1 *= c2;

  h1 ^= k1;
  h1 = ROTL32(h1,13);
  h1 = 5*h1 + c3;

  return h1;
}

static uint32_t
murmur3_mix(uint32_t h1)
{
  h1 ^= h1 >> 16;
  h1 *= 0x85ebca6b;
  h1 ^= h1 >> 13;
  h1 *= 0xc2b2ae35;
  h1 ^= h1 >> 16;

  return h1;
}

int
joqe_build_appendstring(joqe_build *build, int c)
{
  if(!build->first)
    build->current = build->first = joqe_slab_create();

  joqe_slab *s = build->current;

  if(s->write >= STRINGSZ) {
    int sz = s->write - s->mark;
    if(sz + 1 >= STRINGSZ)
      // TODO invalid string, too long; chain strings eventually?
      return 1;

    s->write = s->mark;

    s = build->first;

    do {
      if(!s->nxt)
        s->nxt = joqe_slab_create();
      s = s->nxt;
    } while (sz + 1 >= STRINGSZ - s->write);

    if(sz) {
      memcpy(&s->base[s->mark],
        &build->current->base[build->current->mark], sz);
      s->write += sz;
    }
    build->current = s;
  }
  s->base[s->write++] = (char) c;
  uint32_t block = build->block;
  uint32_t hash = build->hash;
  uint32_t marker = block&MARKERMASK;
  block = (block << 8) | (c & 0xff);
  if(marker == MARKERLAST) {
    build->hash = murmur3_block(hash, block);
    build->block = MARKERFIRST;
  } else {
    build->block = block;
  }

  return 0;
}

const char*
joqe_build_closestring(joqe_build *build)
{
  //check hash, if already exists cancel this one and return old string.
  uint32_t hash = build->hash;
  uint32_t block = build->block;

  //always append last block, even if it's the initial '0x00000001'
  //(i.e. the value size is a multiple of the block size), to ensure
  //that hashes don't accidentally collide due to a value matching the
  //padding.
  hash = murmur3_mix(murmur3_block(hash, block));

  if(joqe_build_appendstring(build, 0))
    return 0;

  char *i = hopscotch_fetch(&build->interned, hash);
  char *c = &build->current->base[build->current->mark];

  if(i && 0 == strcmp(i, c)) {
    joqe_build_cancelstring(build);
    return i;
  }

  // commit string
  build->current->mark = build->current->write;
  if(hopscotch_insert(&build->interned, hash, c))
    D("hash collision: %x %s - %s\n", hash, i, c);
  // ignore/overwrite collisions for now

  joqe_build_reset_hash(build);
  return c;
}

void
joqe_build_cancelstring(joqe_build *build)
{
  build->current->write = build->current->mark;
  build->current = build->first;

  joqe_build_reset_hash(build);
}
