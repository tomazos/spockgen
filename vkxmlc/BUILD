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
      "//sps:spock_api_schema",
      "//sps:spock_api_schema_builder",
      "//vks:vulkan_api_schema",
      "//vks:vulkan_api_schema_parser",
      "//dvc:parser",
      "//dvc:file",
      "//dvc:container",
      "//dvc:string",
  ],
)
