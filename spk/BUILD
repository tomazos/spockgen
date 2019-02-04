package(default_visibility = ["//visibility:public"])

genrule(
  name = "spock_generate",
  srcs = [
	"//data:vk97.xml",
  ],
  outs = [
     "spock.h",
  ],
  cmd = "$(location //vkxmlc) " +
        "--vkxml $(location //data:vk97.xml) " +
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
  deps = [
      "//SDL2",
  ],
  linkopts = [
    "-ldl",
    "-lglog",
  ],
)

