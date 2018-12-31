package(default_visibility = ["//visibility:public"])

cc_library(
  name = "spock_api_schema",
  hdrs = [
    "spock_api_schema.h",
  ],
  deps = [
     "//vks:vulkan_api_schema",
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
