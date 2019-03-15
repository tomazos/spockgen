#pragma once

#include <filesystem>

#include "ft/library.h"
#include "ft/object.h"

namespace ft {

class glyph;

struct char_code_index {
  ULong code;
  UInt index;
};

class face {
 public:
  face(const ft::library& library, std::filesystem::path filepathname,
       Long face_index);

  const FaceRec* operator->() { return face_; }

  void set_char_size(F26Dot6 char_width, F26Dot6 char_height,
                     UInt horz_resolution, UInt vert_resolution);

  void set_pixel_size(UInt pixel_width, UInt pixel_height);

  std::vector<char_code_index> get_char_code_indexs();

  void load_char(ULong char_code, Int32 load_flags = FT_LOAD_RENDER);
  UInt get_char_index(ULong char_code);

  void load_glyph(UInt glyph_index, Int32 load_flags = FT_LOAD_DEFAULT);
  void render_glyph(Render_Mode render_mode);

  Vector get_kerning(UInt left_glyph, UInt right_glyph,
                     UInt kern_mode = FT_KERNING_DEFAULT);

  ft::glyph get_glyph();

 private:
  ft::object<FT_Face, FT_Done_Face> face_;
};

}  // namespace ft
