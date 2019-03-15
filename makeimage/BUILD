package(default_visibility = ["//visibility:public"])

cc_library(
    name = "makeimage",
    srcs = [
        "makeimage.cc",
    ],
    hdrs = [
        "makeimage.h",
    ],
    linkopts = [
        "-lstdc++fs",
        "-lglog",
        "-lgflags",
        "-lpng",
        "-ldl",
    ],
    deps = [
        "//dvc:terminate",
    ],
)

cc_test(
    name = "makeimage_test",
    srcs = [
        "makeimage_test.cc",
    ],
    deps = [
        ":makeimage",
    ],
)
