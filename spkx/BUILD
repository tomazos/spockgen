package(default_visibility = ["//visibility:public"])

cc_library(
    name = "spkx",
    srcs = [
        "frame.cc",
        "program.cc",
        "swapchain.cc",
    ],
    hdrs = [
        "frame.h",
        "helpers.h",
        "program.h",
        "swapchain.h",
    ],
    linkopts = [
        "-lglog",
        "-lgflags",
    ],
    deps = [
        "//dvc:terminate",
        "//spk:spock",
    ],
)

cc_binary(
    name = "spkx_test",
    srcs = [
        "spkx_test.cc",
    ],
    deps = [
        ":spkx",
    ],
)
