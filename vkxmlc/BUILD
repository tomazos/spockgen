package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "vkxmlc",
    srcs = [
        "vkxmlc.cc",
    ],
    linkopts = [
        "-ltinyxml2",
        "-lgflags",
        "-lglog",
        "-lstdc++fs",
    ],
    deps = [
        "//dvc:container",
        "//dvc:file",
        "//dvc:parser",
        "//dvc:string",
        "//sps",
        "//sps:spsbuilder",
        "//vks",
        "//vks:vksparser",
    ],
)
