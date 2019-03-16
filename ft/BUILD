package(default_visibility = ["//visibility:public"])

cc_library(
    name = "ft",
    srcs = [
        "error.cc",
        "face.cc",
        "glyph.cc",
        "library.cc",
    ],
    hdrs = [
        "error.h",
        "face.h",
        "freetype.h",
        "glyph.h",
        "library.h",
        "object.h",
    ],
    linkopts = [
        "-lfreetype",
        "-lstdc++fs",
    ],
    deps = [
        "//dvc:log",
    ],
)

cc_test(
    name = "face_test",
    srcs = [
        "face_test.cc",
    ],
    data = [
        "testdata/FreeSerif.ttf",
    ],
    deps = [
        ":ft",
        "//:gtest",
    ],
)

cc_test(
    name = "glyph_test",
    srcs = [
        "glyph_test.cc",
    ],
    data = [
        "testdata/FreeSerif.ttf",
    ],
    deps = [
        ":ft",
        "//:gtest",
    ],
)
