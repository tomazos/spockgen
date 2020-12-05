load("//:glsl.bzl", "glsl_shader")

genrule(
    name = "vkxmltest_generate",
    srcs = [
        "//data:vk154.xml",
    ],
    outs = [
        "vkxml.json",
        "vkxmltest.cc",
        "vulkan_autogen.h",
        "vulkan_autogen.cc",
    ],
    cmd = "$(location //vkxmlc) " +
          "--vkxml $(location //data:vk154.xml) " +
          "--outtest $(location vkxmltest.cc) " +
          "--outjson $(location vkxml.json) " +
          "--outh $(location vulkan_autogen.h) " +
          "--outcc $(location vulkan_autogen.cc)",
    tools = [
        "//vkxmlc",
    ],
)

cc_library(
    name = "vkxmltest_header",
    hdrs = [
        "vkxmltest.h",
    ],
    deps = [
        "//vulkan",
    ],
)

# cc_test(
#     name = "vkxmltest",
#     srcs = [
#         "vkxmltest.cc",
#     ],
#     deps = [
#         ":vkxmltest_header",
#     ],
# )
