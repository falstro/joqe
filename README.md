joqe - JSON object query expression
===================================
/dʒəʊk/

Joqe is a query language for querying and mapping JSON-type objects onto
an other JSON-type object. Although not strictly limited to JSON, the
language itself is a superset of JSON, making it the most natural
application. In fact, any valid JSON is a valid joqe expression,
producing the JSON itself, regardless of input. Joqe is designed to
scale with complexity, completing simple tasks should be easy and
require a minimal expression.

Joqe takes inspiration from both XPath and JavaScript, and (obviously)
JSON. There are a few fundamental differences, in particular to XPath,
so be careful to pay attention to the caveats below if you're familiar
with XPath.

Path expressions
================

For the following examples, we'll use this as an input document:

    {
      "status": "success",
      "meta": {
        "count": 2,
        "main": 101
        "message": "a message",
        "request-id": "deadbeef"
      },
      "results": [
        {
          "id": 101,
          "name": "one-oh-one",
          "tag": "xyz"
        },
        {
          "id": 103,
          "name": "one-oh-three"
        }
      ]
    }

Simple paths
------------

Paths are constructed similar to how you would select data from an
object in JavaScript. To select the `status` field of the object, we simply
use

    status

Simple enough. To get the count field from the meta object, we'd do

    meta.count

Like javascript, for numeric indexes, or names which are not valid
identifiers, we use `[]` to evaluate arbitrary expressions in order to
determine which child node to pick, and will select all children where
the expression evaluates to true. To get the request ID, we'd use

    meta["request-id"]

where the expression is simply a string literal, which selects all
children (although there's usually only one) with the name matching the
string. In joqe, as in JavaScript, you may use either single or double
quotes to delimit strings. Note that, just as in JavaScript, the
dot-notation above is a short hand, and to get the count field, we could
also use

    meta['count']

Array indexes are zero-based, thus to select the first result, we use

    results[0]

**XPath Caveat**: XPath uses `[]` to denote a predicate of the values
already selected, whereas a joqe `[]` selects a child node of the
selected node(s). This fits better with the JSON type objects, and it
also makes it a better match to JavaScript and other programming
languages, but will seem unnatural at first if you're familiar with
XPath. XML doesn't have the notion of an array, only multiple instances
of the same element. JSON has an array primitive, thus `results` would
be single-valued (the array) in joqe, whereas it would have to be
multi-valued in XPath. To get the first element, in joqe `results[0]`
picks the first element from the array called `results`, whereas in
XPath `results` would select all tags named `results` and `[0]` would
filter out the first one. Although it seems equivalent, it has an impact
on complex queries, such as existence tests (i.e. has-a tests), in
particular when not dealing with arrays.

If the root object is an array and not an object, you cannot use `[0]`
directly, as this is a construct expression (see below) that creates an
array with the zero as the single entry. To select the root element, use
`/`. This tells the parser that we're looking at a path, thus `/[0]`
would select element zero of the root array. This is also valid when
using objects, i.e. `/.meta`, but the root object is implied.

The `/` refers to the current node being evaluated, which in the global
context would be the root node. When evaluating filters, it refers to
the node the filter is being matched against. Consider this matrix document

    [[1,2,3],[4,5,6],[7,8,9]]

we could select all rows (assuming it's row-major order) having an even
number in the second position using

    /[/[1] % 2 = 0]

the outer `/[]` selects all rows, `/[1] % 2 = 0` filters all rows where
the second element modulo 2 equals zero (i.e. is even). Note how `/`
refers to the root array in the first instance and to the sub-array in
the second instance. To refer to the context object in the filter
expression use `//`. The context reference is never implied, and to use
a named path rooted in the context node you'll need to use `//` as well,
e.g. `//.meta`, for example:

    results[id = //.meta.main]

Descendant
----------

Normally, only direct descendants are selected, however, you may select
nodes at arbitrary depths using the descendant operator `..`. The
following would select the two `id` elements (`101`, and `103`)

    ..id

as would this

    results..id

Path unions
-----------

The union of two path expressions can be achieved using the union
operator `|`.The following expression

    status | meta.message

results in two string nodes, `"success"` and `"a message"`. Note
that this is not an array, simply a set of two nodes.

Boolean value of expressions
============================

Formally, the selection criteria inside the `[]` is an expression, and
all child nodes for which the expression is considered true will be
selected.  There are five simple types in joqe: string, integer, real,
boolean, and null, if these are used in a boolean expression they behave
in the following way:

  - String:
    - True if the name of the node matches the string (only object
      fields have names)
    - Allows the natural `object["field"]` access.
  - Integer:
    - True if the index of the node matches the string (only array
      elements have indexes)
    - Allows the natural `array[123]` access.
  - Real:
    - Matches none
    - N.B. matches none even if the real value represents an integer
  - Boolean:
    - `true` matches all, `false` matches none
    - The empty selection `object[]` is equivalent to `object[true]` and
      selects all children of `object` (though not `object` itself).
  - Null:
    - Matches none
  - Objects and arrays:
    - Although rare in boolean contexts, both objects and arrays
      evaluate to true, and would match all.

A path expression does not result in any of the above types, but rather
in a node set, and regardless of what those nodes contain, in a boolean
context they evaluate to false if the set is empty and true if it
contains one or more nodes. The path expression is evaluated relative to
the child node under consideration, and this is where it might get
confusing at first for XPath people, as the following expression (using
the example above) generates the empty set

    meta[message]

In XPath, this would've resulted in the `meta` node. In joqe it's the
empty set. The reason becomes obvious when you consider the definition,
select all children of `meta` for which the expression `message` is
true.  The `message` expression is evaluated with respect to each child
in turn, and there's no child of `meta` with a `message` node in it. Had
it looked like this

    {
      "meta": {
        "status": {
          "message": "abcdefgh"
        },
        "info": {
          "message": "ijklmnop"
        }
      }
    }

the result would've been both the `status` node, and the `info` node, as
both these nodes have a `message` node and thus the expression matches.
The more common case for the `has-a`-path test is selecting elements
from an array, and here it becomes similar to XPath again

    results[tag]

will return all nodes in the array containing a tag (one in this case).

Boolean expressions
-------------------

Joqe supports the common boolean expressions

  - Logical: These evaluate both left and right operands in a boolean
    context.
    - and
    - or
    - not
  - Comparison: These evaluates left and right to determine their value
    - `>`, `>=`: Greater than, and greater than or equal
    - `<`, `<=`: Less than, and less than or equal
    - `=`, `!=`: Equals, and not equals

For the comparisons, all left operands are tested against all right
operands.  This means that the expression matches if any combination
evaluates to true. This in turn means that `<` is not strictly equal to
`not` with `>=`, as the former reads "has any combination less than",
and the latter reads "does not have any combination greater than or
equal to". Same deal with `=` and `not` with `!=`, "has one combination
that is equal" and "no combination which isn't equal"

As usual, expressions can be grouped to override operator precedence
using parentheses `()`.

Value expressions
-----------------

Joqe can do some rudimentary calculations with numeric values

  - `+`, `-`: Addition and subtraction
  - `*`, `/`: Multiplication and division
  - `%`: Remainder/modulo

As with comparisons, these also expand all combinations of left and
right operands in the case of multi-valued expressions, thus the result of

    meta.count + results[].id

is two values, `103` and `105` (again, not an array).

Construct expressions
=====================

Construct expressions construct values, normally objects or arrays.
Similar to JSON, simply enclose an object in curly braces `{}`, and an
array with square brackets `[]`, with each entry separated by a comma
`,`. Object entries are key-value pairs separated by a colon `:`,
whereas array entries are simply values.

Both keys and values are interpreted as expressions. Object and array
constructions have one fundamental difference however: if either key or value
is a multi-valued expression, only the first key and value will be used to
construct the object entry, whereas for arrays, all values produced by
the expression will be used. For example, to use the addition example above

    [meta.count + results[].id]

Will result in the array

    [103, 105]

In addition, if no key is specified for object construction, and the
expression selects an object field, the entry will be constructed with
the name of the field as a key, effectively turning it into a copy
operation. Multi-valued copy expressions will copy all nodes to the
output.

Using the original example from above as input document, we can use the
following expression to construct an output document:

    {
      status,
      "message-id": meta["request-id"],
      "results": [
        results[].id
      ],
      "tagged": [
        results[tag].id
      ],
      "made-by": "joqe"
    }

This will produce the following output, note how plain JSON simply gets
copied to the output:

    {
      "status": "succcess",
      "message-id": "deadbeef",
      "results": [
        101,
        103
      ],
      "tagged": [
        101
      ],
      "made-by": "joqe"
    }

Note that the value of "results" and "tagged" are themselves construct
expressions.  Simply using `results[].id` without constructing an array
here would've resulted in just the first value being selected, i.e.:

      "results": 101

Context expressions
===================

Up to this point, all expressions have been applied to the input
document. Inspired by XQuery, in joqe it is also possible to use other
expressions or construct expressions as context for further expressions,
thus allowing you to construct rather complex queries to the point of
almost turning into a heavy duty data processor.

The context operator `::` tells joqe to use the result of left side as
document when evaluating the right side. The left side is evaluated with
whatever context was set up until now, and if the result is
multi-valued, the right side will be evaluated for each value. We may
construct a document:

    {
      "size": meta.count*2,
      "result": results[0]
    }::{
      "offset": size + result.id,
      result[]
    }

where the first part constructs the document

    {
        "size":   4,
        "result": {
            "id":   101,
            "name": "one-oh-one",
            "tag":  "xyz"
        }
    }

the second part is then applied to that document yielding

    {
        "offset": 105,
        "id":     101,
        "name":   "one-oh-one",
        "tag":    "xyz"
    }

Inside the expression, the "global" context is no longer accessible, if
you need it, remember that the context object is being constructed with
using the outside context, and you can lift the entire object into the
new document using the context object reference `/`, for example

    {
      "size": meta.count * 2,
      "result": results[0],
      "root": /
    }::{
      "offset": size + result.id,
      "main:" root.meta.main,
      result[]
    }

Note that the object is not actually copied, it's simply referenced, so
the operation is cheap and doesn't consume more memory even though the
document might be large. The `result[]` selector will do a shallow copy
of the result object though (i.e. any object within the object will be
referenced, but not the result object entries themselves).

If a context is added to an object entry, it applies to both key and
value for that entry. If the context is multi-valued, they key/value
expressions are applied to each context value, and all results are added
to the output object. This means we can do the object construction

    {results[]:: name: id}

and end up with the following result

    {
      "one-oh-one": 101,
      "one-oh-three": 103
    }

Note that a context used like this only applies to the associated entry,
any following entries would not use the same context. To have the same
context apply to all entries, construct the entire object with it.

    results[0]::{name: id, ... }

The parser
==========

The joqe parser is a lenient parser and allows a number of things a
strict JSON parser would not, both when parsing expressions and when parsing
JSON documents. A few are listed here:

  - Strings can use either single or double quotes
  - Integers can be decimal, octal (prefixed with `0`) or hexadecimal
    (prefixed with `0x`). JSON only accepts a leading `0` when it's a
    single digit, so there's no incompatability here.
  - Reals may lead with a `.`, which implies a leading `0`.
  - Objects and arrays may both contain a `,` after the last element.

Furthermore, C-style comments will be ignored

    /* comment, will be ignored */
    {
      "my": "precious" /* mine, all mine */
    }

The JSON specification is ambiguous whether multiple instances of the
same key are allowed in an object. The specification speaks of a set of
"name/value pairs", but doesn't specify if the key (name) must by unique
or only the key/value pair. Common sense would dictate the former, but
joqe takes the lenient approach and ignores the uniqueness constraint,
and thus will allow multiple entries both with the same name and same
name/value.

While the JSON specification defines an object as a unordered set, which
a number of parsers takes advantage of and randomizes the output order
based on their internal storage structures. Joqe will maintain document
order to the extent possible.

Implementation notes and limitations
------------------------------------

The way the parser is currently organized, there is a hard limit on
string lengths. A single string literal may be no longer than 4Kb at
this time.

The parser can detect and read all JSON specified encodings, i.e. UTF-8,
-16, and -32 both big and little endian, but always stores strings in
UTF-8 internally. It will also interpret and consume any byte order
marker if present (a BOM is however not required to correctly detect the
encoding for any of the five). An unpaired UTF-16 surrogate will result
in a parse error. Currently the only output available is UTF-8 or ASCII.

Integers are mapped to system ints, and thus have limited precision.
Large integer values will likely overflow, but the behavior is
undefined. The same applies to floating point values, which are mapped
to doubles.

String comparisons are currently done on a byte-by-byte basis and are
not locale aware. As mentioned all strings are stored in UTF-8, and all
escaped unicode sequences are parsed, thus `"a\\b"` and `"a\u005Cb"`
will be considered equal.

The parser does not preserve comments or whitespace between nodes.

Implementation pending
======================

There are still a number things waiting to be done, the most prominent
one being to flesh out a number of functions to support, like string
manipulation and type conversions.

Another planned exercise is to create a library and API for integration,
so far the API is a bit convoluted, and needs to be wrapped in something
more manageable.

Stay tuned.
