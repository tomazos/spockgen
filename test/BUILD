genrule(
  name = "vkxmltest_generate",
  srcs = [
	"//data:vk82.xml",
  ],
  outs = [
     "vkxml.json",
     "vkxmltest.cc",
  ],
  cmd = "$(location //vkxmlc) " +
        "--vkxml $(location //data:vk82.xml) " +
        "--outtest $(location vkxmltest.cc) " +
        "--outjson $(location vkxml.json)",
  tools = [
     "//vkxmlc",
  ],
)

cc_binary(
  name = "loadertest",
  srcs = [
     "loadertest.cc",
  ],
  deps = [
     "//spk:spock",
     "//SDL2",
  ],
  linkopts = [
     "-lglog",
     "-lgflags",
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
