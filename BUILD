cc_library(
  name = "libvulkan",
  hdrs = [
    "loader.h",
  ],
  srcs = [
    "loader.cc",
  ],
)

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
      ":spock_api_schema",
      ":spock_api_schema_builder",
      ":vulkan_relaxng",
      ":vulkan_api_schema",
      ":vulkan_api_schema_parser",
      "//dvc:parser",
      "//dvc:file",
      "//dvc:container",
      "//dvc:string",
  ],
)

genrule(
  name = "vulkan_relaxng_generate",
  srcs = [
     "registry.rnc",
  ],
  outs = [
     "vulkan_relaxng.h",
  ],
  cmd = "$(location //relaxng:relaxngc) --namespace vkr --protocol Vulkan82 --schema $(location registry.rnc) --hout $(location vulkan_relaxng.h)",
  tools = [
     "//relaxng:relaxngc",
  ],
)

cc_library(
  name = "vulkan_relaxng",
  hdrs = [
    "vulkan_relaxng.h",
  ],
  deps = [
    "//relaxng",
  ],
)

cc_library(
   name = "vulkan_api_schema",
   hdrs = [
     "vulkan_api_schema.h",
   ],
)

cc_test(
  name = "vulkan_relaxng_test",
  srcs = [
     "vulkan_relaxng_test.cc",
  ],
  deps = [
     ":vulkan_relaxng",
  ],
)

genrule(
  name = "vkxmltest_generate",
  srcs = [
	"vk82.xml",
  ],
  outs = [
     "vkxmltest.cc",
     "vkxml.json",
     "spock.h",
  ],
  cmd = "$(location :vkxmlc) " +
        "--vkxml $(location vk82.xml) " +
        "--outjson $(location vkxml.json) " +
        "--outtest $(location vkxmltest.cc) " +
        "--outh $(location spock.h)",
  tools = [
     ":vkxmlc",
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

cc_library(
  name = "minic_parser",
  hdrs = [
    "minic_parser.h",
  ],
  srcs = [
    "minic_parser.cc",
  ],
  deps = [
    "//dvc:parser",
    "//dvc:scanner",
  ],
)

cc_library(
  name = "vulkan_api_schema_parser",
  hdrs = [
    "vulkan_api_schema_parser.h",
  ],
  srcs = [
    "vulkan_api_schema_parser.cc",
  ],
  deps = [
    ":vulkan_relaxng",
    ":vulkan_api_schema",
    ":minic_parser",
    "//dvc:container",
    "//dvc:string",
  ],
)

cc_library(
  name = "spock",
  hdrs = [
    "spock.h",
  ],
)

cc_test(
  name = "spocktest",
  srcs = [
    "spocktest.cc",
  ],
  deps = [
    ":spock",
  ],
)

cc_library(
  name = "spock_api_schema",
  hdrs = [
    "spock_api_schema.h",
  ],
  deps = [
     ":vulkan_api_schema",
  ],
)

cc_library(
  name = "spock_api_schema_builder",
  hdrs = [
    "spock_api_schema_builder.h",
  ],
  srcs = [
    "spock_api_schema_builder.cc",
  ],
  deps = [
     ":spock_api_schema",
     "//dvc:container",
     "//dvc:string",
  ],
)
