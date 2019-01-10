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
      "//sps",
      "//sps:spsbuilder",
      "//vks",
      "//vks:vksparser",
      "//dvc:parser",
      "//dvc:file",
      "//dvc:container",
      "//dvc:string",
  ],
)
