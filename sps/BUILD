package(default_visibility = ["//visibility:public"])

cc_library(
    name = "sps",
    hdrs = [
        "sps.h",
    ],
    deps = [
        "//dvc:log",
        "//vks",
    ],
)

cc_library(
    name = "spsbuilder",
    srcs = [
        "spsaccessors.cc",
        "spsbuilder.cc",
    ],
    hdrs = [
        "spsaccessors.h",
        "spsbuilder.h",
    ],
    deps = [
        ":sps",
        "//dvc:container",
        "//dvc:string",
    ],
)
