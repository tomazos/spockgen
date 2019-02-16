package(default_visibility = ["//visibility:public"])

genrule(
    name = "spock_generate",
    srcs = [
        "//data:vk97.xml",
    ],
    outs = [
        "spock.h",
    ],
    cmd = "$(location //vkxmlc) " +
          "--vkxml $(location //data:vk97.xml) " +
          "--outh $(location spock.h)",
    tools = [
        "//vkxmlc",
    ],
)

cc_library(
    name = "spock",
    srcs = [
        "loader.cc",
    ],
    hdrs = [
        "loader.h",
        "spock.h",
        "spock_fwd.h",
    ],
    linkopts = [
        "-ldl",
        "-lglog",
        "-lSDL2",
    ],
)
