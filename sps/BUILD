package(default_visibility = ["//visibility:public"])

cc_library(
  name = "sps",
  hdrs = [
    "sps.h",
  ],
  deps = [
     "//vks",
  ],
)

cc_library(
  name = "spsbuilder",
  hdrs = [
    "spsbuilder.h",
    "spsaccessors.h",
  ],
  srcs = [
    "spsbuilder.cc",
    "spsaccessors.cc",
  ],
  deps = [
     ":sps",
     "//dvc:container",
     "//dvc:string",
  ],
)
