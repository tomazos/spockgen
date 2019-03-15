#include "ft/face.h"

#include "gtest/gtest.h"

namespace {

TEST(FaceTest, Open) {
  ft::library library;
  ft::face face(library, "ft/testdata/FreeSerif.ttf", 0);
  EXPECT_EQ(face->num_glyphs, 10538);
}

}  // namespace
