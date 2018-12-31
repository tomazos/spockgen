package(default_visibility = ["//visibility:public"])

cc_library(
  name = "relaxng",
  hdrs = [
    "relaxng.h",
  ],
  deps = [
      "//dvc:json",
  ],
)

cc_binary(
  name = "relaxngc",
  srcs = [
    "relaxngc.cc",
  ],
  linkopts = [
    "-lgflags",
    "-lglog",
    "-lstdc++fs",
  ],
  deps = [
    "//dvc:file",
    "//dvc:parser",
    "//dvc:scanner",
    "//dvc:json",
  ],
)
