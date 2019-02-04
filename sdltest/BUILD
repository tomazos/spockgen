package(default_visibility = ["//visibility:public"])

cc_library(
    name = "sdltest_hdrs",
    hdrs = glob(["*.h"]),
)

cc_binary(
    name = "sdltest",
    srcs = glob(["*.cc"]),
    deps = [
	":sdltest_hdrs",

	],
    data = [
		    "vert.spv",
		    "frag.spv",
		    ],
    linkopts =[
		    "-lglog",
		    "-lvulkan",
		    "-lgflags",
		    "-lstdc++fs",
		    "-lSDL2",
		    ],
)

genrule(
    name = "compile_vert_shader",
    srcs = [
        "shader.vert",
    ],
    outs = [
        "vert.spv",
    ],
    cmd = "./$(location glslc) -V -o $@ $(SRCS)",
    tools = [
        "glslc",
    ],
)

genrule(
    name = "compile_frag_shader",
    srcs = [
        "shader.frag",
    ],
    outs = [
        "frag.spv",
    ],
    cmd = "./$(location glslc) -V -o $@ $(SRCS)",
    tools = [
        "glslc",
    ],
)

