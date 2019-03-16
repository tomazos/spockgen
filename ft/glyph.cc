#include "ft/glyph.h"

#include "dvc/log.h"

namespace ft {

glyph::glyph(const glyph& original) : glyph_(FT_Glyph_Copy, original.glyph_) {}

glyph& glyph::operator=(const glyph& original) {
  glyph temp(original);
  *this = std::move(temp);
  return *this;
}

void glyph::convert_to_bitmap(FT_Render_Mode render_mode, FT_Vector origin) {
  throw_on_error(FT_Glyph_To_Bitmap(glyph_.ptr(), render_mode, &origin,
                                    true /* destroy */));
}

bool glyph::is_bitmap() const {
  return glyph_->format == FT_GLYPH_FORMAT_BITMAP;
}

bool glyph::is_outline() const {
  return glyph_->format == FT_GLYPH_FORMAT_OUTLINE;
}

FT_BitmapGlyphRec& glyph::bitmap() {
  DVC_ASSERT(is_bitmap());
  FT_Glyph glyph = glyph_;
  FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)glyph;
  DVC_ASSERT(bitmap_glyph);
  return *bitmap_glyph;
}

const FT_BitmapGlyphRec& glyph::bitmap() const {
  DVC_ASSERT(is_bitmap());
  const FT_GlyphRec* glyph = glyph_;
  const FT_BitmapGlyphRec* bitmap_glyph = (const FT_BitmapGlyphRec*)glyph;
  DVC_ASSERT(bitmap_glyph);
  return *bitmap_glyph;
}

FT_OutlineGlyphRec& glyph::outline() {
  DVC_ASSERT(is_outline());
  FT_Glyph glyph = glyph_;
  FT_OutlineGlyph outline_glyph = (FT_OutlineGlyph)glyph;
  DVC_ASSERT(outline_glyph);
  return *outline_glyph;
}

const FT_OutlineGlyphRec& glyph::outline() const {
  DVC_ASSERT(is_outline());
  const FT_GlyphRec* glyph = glyph_;
  const FT_OutlineGlyphRec* outline_glyph = (const FT_OutlineGlyphRec*)glyph;
  DVC_ASSERT(outline_glyph);
  return *outline_glyph;
}

glyph::glyph(FT_GlyphSlot slot) : glyph_(FT_Get_Glyph, slot) {}

FT_Error glyph::done_glyph(FT_Glyph glyph) {
  FT_Done_Glyph(glyph);
  return 0;
}

}  // namespace ft
