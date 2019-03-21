load("//:glsl.bzl", "glsl_shader")

cc_binary(
    name = "fonttest",
    srcs = [
        "fonttest.cc",
    ],
    linkopts = [
        "-lpng",
    ],
    deps = [
        "//dvc:log",
        "//dvc:program",
        "//ft",
    ],
)

cc_binary(
    name = "ranwalkthing",
    srcs = [
        "ranwalkthing.cc",
    ],
    deps = [
        "//makeimage",
    ],
)

#cc_binary(
#    name = "starfield",
#    srcs = [
#        "starfield.cc",
#    ],
#    deps = [
#        "//makeimage",
#    ],
#)

genrule(
    name = "vkxmltest_generate",
    srcs = [
        "//data:vk97.xml",
    ],
    outs = [
        "vkxml.json",
        "vkxmltest.cc",
    ],
    cmd = "$(location //vkxmlc) " +
          "--vkxml $(location //data:vk97.xml) " +
          "--outtest $(location vkxmltest.cc) " +
          "--outjson $(location vkxml.json)",
    tools = [
        "//vkxmlc",
    ],
)

glsl_shader(
    name = "triangletest1_vert",
    src = "triangletest1.vert",
)

glsl_shader(
    name = "triangletest1_frag",
    src = "triangletest1.frag",
)

glsl_shader(
    name = "pointtest_vert",
    src = "pointtest.vert",
)

glsl_shader(
    name = "pointtest_frag",
    src = "pointtest.frag",
)

glsl_shader(
    name = "skyfly_vert",
    src = "skyfly.vert",
)

glsl_shader(
    name = "skyfly_frag",
    src = "skyfly.frag",
)

cc_binary(
    name = "skyfly",
    srcs = [
        "skyfly.cc",
    ],
    data = [
        "skyfly.frag.spv",
        "skyfly.vert.spv",
    ],
    linkopts = [
        "-lgflags",
        "-lpng",
    ],
    deps = [
        "//dvc:file",
        "//dvc:terminate",
        "//spk:spock",
        "//spkx",
    ],
)

cc_binary(
    name = "pointtest",
    srcs = [
        "pointtest.cc",
    ],
    data = [
        "pointtest.frag.spv",
        "pointtest.vert.spv",
    ],
    linkopts = [
        "-lgflags",
    ],
    deps = [
        "//dvc:file",
        "//dvc:terminate",
        "//spk:spock",
        "//spkx",
    ],
)

cc_binary(
    name = "triangletest1",
    srcs = [
        "triangletest1.cc",
    ],
    data = [
        "triangletest1.frag.spv",
        "triangletest1.vert.spv",
    ],
    linkopts = [
        "-lgflags",
    ],
    deps = [
        "//dvc:file",
        "//dvc:opts",
        "//dvc:terminate",
        "//spk:spock",
    ],
)

cc_test(
    name = "vulkan_relaxng_test",
    srcs = [
        "vulkan_relaxng_test.cc",
    ],
    deps = [
        "//vks:vulkan_relaxng",
    ],
)

cc_library(
    name = "vkxmltest_header",
    hdrs = [
        "vkxmltest.h",
    ],
)

cc_test(
    name = "vkxmltest",
    srcs = [
        "vkxmltest.cc",
    ],
    deps = [
        ":vkxmltest_header",
    ],
)

cc_test(
    name = "spocktest",
    srcs = [
        "spocktest.cc",
    ],
    deps = [
        "//spk:spock",
    ],
)

cc_binary(
    name = "vulkan_debug_test",
    srcs = [
        "vulkan_debug_test.c",
    ],
    linkopts = [
        "-lvulkan",
    ],
)
