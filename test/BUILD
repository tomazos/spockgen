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

genrule(
  name = "triangletest_vert_generate",
  srcs = [
	"triangletest.vert",
  ],
  outs = [
     "triangletest.vert.spv",
  ],
  cmd = "glslc $(location triangletest.vert) " +
        "-o $(location triangletest.vert.spv) ",
        
)

genrule(
  name = "triangletest_frag_generate",
  srcs = [
	"triangletest.frag",
  ],
  outs = [
     "triangletest.frag.spv",
  ],
  cmd = "glslc $(location triangletest.frag) " +
        "-o $(location triangletest.frag.spv) ",
        
)

cc_binary(
  name = "triangletest",
  srcs = [
     "triangletest.cc",
  ],
  deps = [
     "//spk:spock",
     "//dvc:file",
  ],
  data = [
     "triangletest.vert.spv",
     "triangletest.frag.spv",
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
