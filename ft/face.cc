#include "ft/face.h"

#include <cstring>

#include "ft/glyph.h"

namespace ft {

face::face(const ft::library& library, std::filesystem::path filepathname,
           FT_Long face_index)
    : face_(FT_New_Face, library, filepathname.string().c_str(), face_index) {}

void face::set_char_size(FT_F26Dot6 char_width, FT_F26Dot6 char_height,
                         FT_UInt horz_resolution, FT_UInt vert_resolution) {
  throw_on_error(FT_Set_Char_Size(face_, char_width, char_height,
                                  horz_resolution, vert_resolution));
}

void face::set_pixel_size(FT_UInt pixel_width, FT_UInt pixel_height) {
  throw_on_error(FT_Set_Pixel_Sizes(face_, pixel_width, pixel_height));
}

FT_UInt face::get_char_index(FT_ULong char_code) {
  return FT_Get_Char_Index(face_, char_code);
}

void face::load_char(FT_ULong char_code, FT_Int32 load_flags) {
  throw_on_error(FT_Load_Char(face_, char_code, load_flags));
}

void face::load_glyph(FT_UInt glyph_index, FT_Int32 load_flags) {
  throw_on_error(FT_Load_Glyph(face_, glyph_index, load_flags));
}

void face::render_glyph(FT_Render_Mode render_mode) {
  throw_on_error(FT_Render_Glyph(face_->glyph, render_mode));
}

FT_Vector face::get_kerning(FT_UInt left_glyph, FT_UInt right_glyph,
                            FT_UInt kern_mode) {
  FT_Vector v;
  std::memset(&v, 0, sizeof(v));
  throw_on_error(FT_Get_Kerning(face_, left_glyph, right_glyph, kern_mode, &v));
  return v;
}

ft::glyph face::get_glyph() { return {face_->glyph}; }

std::vector<char_code_index> face::get_char_code_indexs() {
  std::vector<char_code_index> result;
  FT_ULong charcode;
  FT_UInt gindex;

  charcode = FT_Get_First_Char(face_, &gindex);
  while (gindex != 0) {
    result.push_back({charcode, gindex});

    charcode = FT_Get_Next_Char(face_, charcode, &gindex);
  }
  return result;
}

}  // namespace ft
