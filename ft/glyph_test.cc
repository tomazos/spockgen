#include "ft/glyph.h"

#include "ft/face.h"
#include "gtest/gtest.h"

namespace {

class GlyphTest : public testing::Test {
 public:
  GlyphTest() : face(library, "ft/testdata/FreeSerif.ttf", 0) {}

  ft::library library;
  ft::face face;
};

TEST_F(GlyphTest, Smoke) {
  face.set_pixel_size(160, 160);
  face.load_char('A');
  ft::glyph glyph = face.get_glyph();
  ft::glyph glyph2(glyph);
}

TEST_F(GlyphTest, Render) {
  face.set_pixel_size(160, 160);
  face.load_char('A', FT_LOAD_DEFAULT);
  ft::glyph glyph = face.get_glyph();
  EXPECT_TRUE(glyph.is_outline());
  EXPECT_FALSE(glyph.is_bitmap());
  ft::glyph glyph2(glyph);
  glyph.convert_to_bitmap();
  EXPECT_FALSE(glyph.is_outline());
  EXPECT_TRUE(glyph.is_bitmap());
  EXPECT_TRUE(glyph2.is_outline());
  EXPECT_FALSE(glyph2.is_bitmap());
}

}  // namespace
