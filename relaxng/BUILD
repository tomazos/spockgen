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
        "-lstdc++fs",
    ],
    deps = [
        "//dvc:file",
        "//dvc:json",
        "//dvc:opts",
        "//dvc:parser",
        "//dvc:scanner",
    ],
)
