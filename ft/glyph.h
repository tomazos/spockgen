#pragma once

#include "ft/object.h"

namespace ft {

class glyph {
 public:
  glyph(const glyph& original);

  glyph& operator=(const glyph& original);

  void convert_to_bitmap(Render_Mode render_mode = FT_RENDER_MODE_NORMAL,
                         Vector origin = {0, 0});

  bool is_bitmap() const;
  bool is_outline() const;

  Glyph_Format format() const { return glyph_->format; }
  Vector advance() const { return glyph_->advance; }

  BitmapGlyphRec& bitmap();
  const BitmapGlyphRec& bitmap() const;
  OutlineGlyphRec& outline();
  const OutlineGlyphRec& outline() const;

 private:
  glyph(FT_GlyphSlot slot);

  static FT_Error done_glyph(FT_Glyph glyph);

  ft::object<FT_Glyph, done_glyph> glyph_;

  friend class face;
};

}  // namespace ft
