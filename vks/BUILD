package(default_visibility = ["//visibility:public"])

genrule(
  name = "vulkan_relaxng_generate",
  srcs = [
     "//data:registry.rnc",
  ],
  outs = [
     "vulkan_relaxng.h",
  ],
  cmd = "$(location //relaxng:relaxngc) --namespace vkr --protocol Vulkan82 --schema $(location //data:registry.rnc) --hout $(location vulkan_relaxng.h)",
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
    "//mnc:minic_parser",
    "//dvc:container",
    "//dvc:string",
  ],
)
