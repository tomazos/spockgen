#include "ft/error.h"

#include <map>

namespace ft {

#undef __FTERRORS_H__

#define FT_ERRORDEF(e, v, s) \
  case e:                    \
    return s;
#define FT_ERROR_START_LIST
#define FT_ERROR_END_LIST

const char* error_to_string(Error error) {
  switch (error) {
#include FT_ERRORS_H

    default:
      return "unknown error";
  }
}

void throw_on_error(Error error) {
  if (error != 0) throw std::runtime_error(error_to_string(error));
}

}  // namespace ft
