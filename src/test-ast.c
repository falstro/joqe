#include "ast.h"
#include "lex-source.h"
#include "build.h"
#include "joqe.tab.h"
#include "lex.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

int check(const char *exp, joqe_node *in, const char *out);

const char *testDocument = "{"
  "'status':'success',"
  "'message':'ok',"
  "'meta': {"
    "'priority': 1,"
    "'sequence': 3245,"
    "'frequency': 121.9,"
    "'tags': ['ok','information']"
  "},"
  "'results': ["
    "{'color':'red','hex':'#f00','tags':['warning','error','failure']},"
    "{'color':'green','hex':'#0f0','tags':['ok','success']},"
    "{'color':'blue','hex':'#00f','tags':['information']},"
    "{'color':'cyan','hex':'#0ff'},"
    "{'color':'magenta','hex':'#f0f'},"
    "{'color':'yellow','hex':'#ff0'},"
    "{'color':'black','hex':'#000'},"
  "]"
"}";

int cases(joqe_node *doc)
{
  return check("'x'",doc,"'x'")
      || check("1",doc,"1")
      || check("{status}",doc,"{'status':'success'}")
      || check("[results[tags[] > 'warming']::color]", doc, "['red']")
      || check("..[color < 'c' and tags[] = //meta.tags[1]]::color", doc,
        "'blue'")
      || check("{..[color < 'c' and tags[] = //meta.tags[1]]::color:hex}", doc,
        "{'blue':'#00f'}")
      || check("[meta[. > 100]]", doc, "[3245,121.9]")
      || check("[(results[tags[] > 'warming']::color)]", doc, "['red']")
      || check("[results[tags[] > 'warming'].color]", doc, "['red']")
      || check("[results[tags[] = //meta.tags[0]].color]", doc, "['green']")
      || check("[results[0 or 1].color]", doc, "['red','green']")
      || check("results[0 and 1] or null", doc, "null")
      || check("results[0].color", doc, "'red'")
      || check("results[0 and color].color", doc, "'red'")
      || check("results[0].color or null", doc, "'red'")
  ;
}

int fail(const char *msg, ...)
{
  va_list ap;
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  return -1;
}

int
main(int argc, char **argv)
{
  if(argc > 1) {
    printf("%s\n", testDocument);
    return 0;
  }
  joqe_build inb  = joqe_build_init(joqe_lex_source_string(testDocument));
  if (joqe_json(&inb)) return fail("Unable to parse input: %s", testDocument);

  int r = cases(&inb.root.u.node);

  joqe_build_destroy(&inb);
  return r;
}

int
joqe_yyerror(joqe_build *build, const char *msg)
{
  fail("Parsing failed: %s", msg);
  return 0;
}

int equal(joqe_nodels *actual, joqe_nodels *expected)
{
  int r;
  joqe_nodels *aend = actual,
              *eend = expected;
  if(actual && !expected) {
    return fail("Found something but expected nothing");
  } else if(!actual && expected) {
    return fail("Found nothing but expected something");
  }

  do {
    joqe_type t, et;
    if((t = actual->n.type) != (et = expected->n.type)
      && (JOQE_TYPE_KEY(et) != joqe_type_broken
        ||JOQE_TYPE_VALUE(et) != JOQE_TYPE_VALUE(t))
      )
      return fail("Type missmatch: 0x%x (expected: 0x%x)",
        actual->n.type, expected->n.type);
    else if(JOQE_TYPE_KEY(t) == joqe_type_string_none
          && JOQE_TYPE_KEY(et) == joqe_type_string_none
          && 0 != strcmp(actual->n.k.key, expected->n.k.key))
        return fail("Key missmatch: %s (expected: %s)",
          actual->n.k.key, expected->n.k.key);
    else switch(JOQE_TYPE_VALUE(t)) {
      case joqe_type_none_string:
        if(0 != strcmp(actual->n.u.s, expected->n.u.s))
          return fail("Value missmatch: \"%s\" (expected: \"%s\")",
            actual->n.u.s, expected->n.u.s);
        break;
      case joqe_type_none_integer:
        if(actual->n.u.i != expected->n.u.i)
          return fail("Value missmatch: %d (expected: %d)",
            actual->n.u.i, expected->n.u.i);
        break;
      case joqe_type_none_real:
        if(actual->n.u.d != expected->n.u.d)
          return fail("Value missmatch: %lf (expected: %lf)",
            actual->n.u.d, expected->n.u.d);
        break;
      case joqe_type_none_object:
      case joqe_type_none_array:
        if((r = equal(actual->n.u.ls, expected->n.u.ls)))
          return r;
        break;
    }

    actual = (joqe_nodels*) actual->ll.n;
    expected = (joqe_nodels*) expected->ll.n;
  } while(actual != aend && expected != eend);

  if(actual == aend) {
    if(expected == eend)
      return 0;
    else
      return fail("Extraneous nodes");
  }
  else
    return fail("Missing nodes");
}

int check(const char *exp, joqe_node *in, const char *out)
{
  joqe_build expb = joqe_build_init(joqe_lex_source_string(exp));
  joqe_build outb = joqe_build_init(joqe_lex_source_string(out));

  if (joqe_json(&outb)) return fail("Unable to parse output: %s", out);
  if (joqe_yyparse(&expb)) return fail("Unable to parse expression: %s", exp);

  joqe_result jr = {};
  expb.root.construct(&expb.root, in, in, &jr);

  joqe_nodels outls = {.n = outb.root.u.node};
  outls.ll.n = outls.ll.p = &outls.ll;

  int r = equal(jr.ls, &outls);
  if(r) fail("Expectation failed for '%s'", exp);

  joqe_result_destroy(&jr);

  joqe_build_destroy(&outb);
  joqe_build_destroy(&expb);
  return r;
}

