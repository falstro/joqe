#include "lex.h"

#include "err.h"

const char*
joqe_invalid_string(int code)
{
  switch(code) {
    case INVALID_END_OF_INPUT:
      return "End of input in string";
    case INVALID_OVERLONG:
      return "String too long";
    case INVALID_CONTROL_CHARACTER:
      return "String contains control characters";
    case INVALID_UNICODE_ESCAPE:
      return "String contains an invalid unicode escape sequence";
    case INVALID_UNICODE_SURROGATE:
      return "String contains an invalid unicode surrogate pair";
    default:
      return "Unknown string parsing error";
  }
}
