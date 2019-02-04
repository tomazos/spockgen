package(default_visibility = ["//visibility:public"])

cc_library(
        name = "SDL2",
        hdrs = glob(["*.h"]),
        srcs = [
                        "libSDL2.a",
        ],
        linkopts = [
          "-lm",
          "-ldl",
          "-lsndio",
          "-lpthread",
          "-lrt"
          ],
        
)


