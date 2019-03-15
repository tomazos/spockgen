#pragma once

#include <freetype2/ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

namespace ft {

using Long = FT_Long;
using FaceRec = FT_FaceRec;
using F26Dot6 = FT_F26Dot6;
using UInt = FT_UInt;
using ULong = FT_ULong;
using Render_Mode = FT_Render_Mode;
using Error = FT_Error;
using Int32 = FT_Int32;
using Vector = FT_Vector;
using Glyph_Format = FT_Glyph_Format;
using BitmapGlyphRec = FT_BitmapGlyphRec;
using OutlineGlyphRec = FT_OutlineGlyphRec;

}  // namespace ft
