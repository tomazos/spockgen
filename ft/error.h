#pragma once

#include "ft/freetype.h"

namespace ft {

const char* error_to_string(Error);

void throw_on_error(Error);

}  // namespace ft
