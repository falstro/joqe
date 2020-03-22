#ifndef __JOQE_JSON_H__
#define __JOQE_JSON_H__

#include "util.h"

#include <stdint.h>

#define JOQE_TYPE_VALUE_MASK  0x0f
#define JOQE_TYPE_KEY_MASK    0x30

#define JOQE_TYPE_KEY_NONE    0x00
#define JOQE_TYPE_KEY_STRING  0x10
#define JOQE_TYPE_KEY_INT     0x20

#define JOQE_TYPE(k,v)        (JOQE_TYPE_KEY(k)|JOQE_TYPE_VALUE(v))
#define JOQE_TYPE_VALUE(t)    (JOQE_TYPE_VALUE_MASK&(t))
#define JOQE_TYPE_KEY(t)      (JOQE_TYPE_KEY_MASK&(t))
typedef enum {
  joqe_type_broken       = 0x00,
  joqe_type_none_true    = 0x01,
  joqe_type_none_false   = 0x02,
  joqe_type_none_null    = 0x03,
  joqe_type_none_string  = 0x04,
  joqe_type_none_integer = 0x05,
  joqe_type_none_real    = 0x06,
  joqe_type_none_object  = 0x07,
  joqe_type_none_array   = 0x08,
  joqe_type_string_none    = 0x00|JOQE_TYPE_KEY_STRING,
  joqe_type_string_true    = 0x01|JOQE_TYPE_KEY_STRING,
  joqe_type_string_false   = 0x02|JOQE_TYPE_KEY_STRING,
  joqe_type_string_null    = 0x03|JOQE_TYPE_KEY_STRING,
  joqe_type_string_string  = 0x04|JOQE_TYPE_KEY_STRING,
  joqe_type_string_integer = 0x05|JOQE_TYPE_KEY_STRING,
  joqe_type_string_real    = 0x06|JOQE_TYPE_KEY_STRING,
  joqe_type_string_object  = 0x07|JOQE_TYPE_KEY_STRING,
  joqe_type_string_array   = 0x08|JOQE_TYPE_KEY_STRING,
  joqe_type_int_none    = 0x00|JOQE_TYPE_KEY_INT,
  joqe_type_int_true    = 0x01|JOQE_TYPE_KEY_INT,
  joqe_type_int_false   = 0x02|JOQE_TYPE_KEY_INT,
  joqe_type_int_null    = 0x03|JOQE_TYPE_KEY_INT,
  joqe_type_int_string  = 0x04|JOQE_TYPE_KEY_INT,
  joqe_type_int_integer = 0x05|JOQE_TYPE_KEY_INT,
  joqe_type_int_real    = 0x06|JOQE_TYPE_KEY_INT,
  joqe_type_int_object  = 0x07|JOQE_TYPE_KEY_INT,
  joqe_type_int_array   = 0x08|JOQE_TYPE_KEY_INT,
  joqe_type_ref_cnt     = 0xfe
} joqe_type;

typedef struct joqe_nodels joqe_nodels;

typedef struct {
  joqe_type type;
  union {
    const char *key;
    int         idx;
  } k;
  union {
    const char   *s;
    int64_t       i;
    double        d;
    joqe_nodels *ls;
  } u;
} joqe_node;

struct joqe_nodels {
  joqe_list ll;
  joqe_node n;
};

struct joqe_build;
int joqe_json (struct joqe_build *b);

#endif /* idempotent include guard */
