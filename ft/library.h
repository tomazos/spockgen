#pragma once

#include "ft/object.h"

namespace ft {

class library {
 public:
  library();

  operator FT_Library() const { return library_; }

 private:
  object<FT_Library, FT_Done_FreeType> library_;
};

}  // namespace ft
