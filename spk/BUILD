package(default_visibility = ["//visibility:public"])

genrule(
  name = "spock_generate",
  srcs = [
	"//data:vk82.xml",
  ],
  outs = [
     "spock.h",
  ],
  cmd = "$(location //vkxmlc) " +
        "--vkxml $(location //data:vk82.xml) " +
        "--outh $(location spock.h)",
  tools = [
     "//vkxmlc",
  ],
)

cc_library(
  name = "spock",
  hdrs = [
    "loader.h",
    "spock_fwd.h",
    "spock.h",
  ],
  srcs = [
      "loader.cc",
  ],
  linkopts = [
    "-ldl",
    "-lglog",
  ],
)

