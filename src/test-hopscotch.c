#include "hopscotch.h"

#include <assert.h>
#include <stdlib.h>

int
main(void)
{
#define INTS 16
  int i, *as = calloc(INTS, sizeof(int));
  hopscotch h = hopscotch_create();
  for(i = 0; i < INTS; ++i) {
    as[i] =
      i*i;
      //(i+3)*7;
      //i;
    hopscotch_insert(&h, as[i], as+i);
    assert("immediate" && hopscotch_fetch(&h, as[i]) == as+i);
  }

  for(i = 0; i < INTS; ++i) {
    assert("after" && hopscotch_fetch(&h, as[i]) == as+i);
  }

  hopscotch_destroy(&h);
  free(as);
  return 0;
}
