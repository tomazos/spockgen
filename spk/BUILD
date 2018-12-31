package(default_visibility = ["//visibility:public"])

cc_library(
  name = "loader",
  hdrs = [
    "loader.h",
  ],
  srcs = [
    "loader.cc",
  ],
)

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
    "spock.h",
  ],
)

