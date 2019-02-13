def glsl_shader(name, src):
  native.genrule(
    name = name,
    srcs = [
    src,
  ],
  outs = [
    src + ".spv",
  ],
  cmd = "glslc $(location %s) -o $(location %s.spv)" % (src,src) 
)
