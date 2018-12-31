#include <gflags/gflags.h>
#include <glog/logging.h>
#include <iostream>
#include <set>
#include <unordered_set>

#include "dvc/container.h"
#include "dvc/file.h"
#include "dvc/string.h"

#include "vulkanhpp/spock_api_schema.h"
#include "vulkanhpp/spock_api_schema_builder.h"
#include "vulkanhpp/vulkan_api_schema.h"
#include "vulkanhpp/vulkan_api_schema_parser.h"
#include "vulkanhpp/vulkan_relaxng.h"

DEFINE_string(vkxml, "", "Input vk.xml file");
DEFINE_string(outjson, "", "Output AST to json");
DEFINE_string(outtest, "", "Output test of API");
DEFINE_string(outh, "", "Output C++ header");

void write_test(const vks::Registry& registry) {
  dvc::file_writer test(FLAGS_outtest, dvc::truncate);

  test.println("#include \"vulkanhpp/vkxmltest.h\"");

  test.println("//enums");

  for (const auto& [name, constant] : registry.constants) {
    if (constant->platform)
      test.println("#ifdef ", constant->platform->protect);
    test.println("VKXMLTEST_CHECK_CONSTANT(", name, ", ", constant->value,
                 ");");
    if (constant->platform) test.println("#endif");
  }

  for (const auto& [name, enumeration] : registry.enumerations) {
    test.println("VKXMLTEST_CHECK_ENUMERATION(", name, ");");
    for (const vks::Constant* enumerator : enumeration->enumerators)
      test.println("VKXMLTEST_CHECK_ENUMERATOR(", name, ", ", enumerator->name,
                   ")");
  }

  for (const auto& [name, bitmask] : registry.bitmasks) {
    if (bitmask->platform) test.println("#ifdef ", bitmask->platform->protect);
    test.println("VKXMLTEST_CHECK_BITMASK(", name, ");");
    if (bitmask->requires)
      test.println("VKXMLTEST_CHECK_BITMASK_REQUIRES(", name, ", ",
                   bitmask->requires->name, ");");
    if (bitmask->platform) test.println("#endif");
  }

  for (const auto& [name, handle] : registry.handles) {
    test.println("VKXMLTEST_CHECK_HANDLE(", name, ");");
    for (const auto& parent : handle->parents) {
      test.println("VKXMLTEST_CHECK_HANDLE_PARENT(", name, ", ", parent->name,
                   ");");
    }
  }

  for (const auto& [name, struct_] : registry.structs) {
    if (struct_->platform) test.println("#ifdef ", struct_->platform->protect);
    test.println("VKXMLTEST_CHECK_STRUCT(", name, ", ", struct_->is_union,
                 ");");
    for (const auto& member : struct_->members) {
      test.println("VKXMLTEST_CHECK_STRUCT_MEMBER(", name, ", ", member.name,
                   ", ", member.type->to_string(), ");");
    }
    if (struct_->platform) test.println("#endif");
  }

  for (const auto& [name, funcpointer] : registry.function_prototypes) {
    test.println("VKXMLTEST_CHECK_FUNCPOINTER(", name, ", ",
                 funcpointer->to_type_string(), ");");
  }

  for (const auto& [name, command] : registry.commands) {
    (void)command;
    if (command->platform) test.println("#ifdef ", command->platform->protect);
    test.println("VKXMLTEST_CHECK_COMMAND(", name, ", ",
                 command->to_type_string(), ");");
    if (command->platform) test.println("#endif");
  }

  test.println("VKXMLTEST_MAIN");
}

void write_header(const sps::Registry& registry) {
  dvc::file_writer h(FLAGS_outh, dvc::truncate);

  h.println("#pragma once");
  h.println();
  h.println("#include <vulkan/vulkan.h>");
  h.println();
  h.println("namespace spk {");
  h.println();

  for (const sps::Bitmask* bitmask : registry.bitmasks) {
    std::string name = bitmask->name;
    h.println("// bitmask ", bitmask->bitmask->name);
    h.print("enum class ", name, " {");
    if (bitmask->enumerators.empty()) {
      h.println("};");
      h.println();
    } else {
      h.println();
      for (const auto& enumerator : bitmask->enumerators)
        h.println("  ", enumerator.name, " = ", enumerator.constant->name, ",");
      h.println("};");
      h.println("inline ", name, " operator~(", name, " a){ return ", name,
                "(~VkFlags(a));}");
      h.println("inline ", name, " operator|(", name, " a, ", name,
                " b){ return ", name, "(VkFlags(a) | VkFlags(b));}");
      h.println("inline ", name, " operator&(", name, " a, ", name,
                " b){ return ", name, "(VkFlags(a) & VkFlags(b));}");
      h.println("inline ", name, " operator^(", name, " a, ", name,
                " b){ return ", name, "(VkFlags(a) ^ VkFlags(b));}");
      h.println();
    }
    for (const auto& alias : bitmask->aliases) {
      h.println("using ", alias, " = ", bitmask->name, ";");
      h.println();
    }
  }

  for (const sps::Enumeration* enumeration : registry.enumerations) {
    h.println("// enumeration ", enumeration->enumeration->name);
    h.println("enum class ", enumeration->name, " {");
    for (const auto& enumerator : enumeration->enumerators)
      h.println("  ", enumerator.name, " = ", enumerator.constant->name, ",");
    h.println("};");
    h.println();
    for (const auto& alias : enumeration->aliases) {
      h.println("using ", alias, " = ", enumeration->name, ";");
      h.println();
    }
  }

  for (const auto& constant : registry.constants) {
    if (constant->constant->platform)
      h.println("#ifdef ", constant->constant->platform->protect);
    h.println("constexpr auto ", constant->name, " = ",
              constant->constant->name, ";");
    if (constant->constant->platform) h.println("#endif");
  }

  h.println();

  for (const auto& handle : registry.handles) {
    h.println("// handle ", handle->handle->name);
    h.println("struct ", handle->name, " {");
    h.println("  ", handle->handle->name, " handle = VK_NULL_HANDLE;");
    h.println();
    for (const auto& command : handle->commands) {
      if (command->command->platform)
        h.println("#ifdef ", command->command->platform->protect);
      h.println("  void ", command->name, "();");
      if (command->command->platform) h.println("#endif");
    }
    h.println("};");
    for (const auto& alias : handle->aliases) {
      h.println("using ", alias, " = ", handle->name, ";");
      h.println();
    }
    h.println();
  }

  for (const auto& struct_ : registry.structs) {
    if (struct_->struct_->platform)
      h.println("#ifdef ", struct_->struct_->platform->protect);
    h.println("// struct ", struct_->struct_->name);
    h.println("struct ", struct_->name, " {");
    h.println("  using underlying_type = ", struct_->struct_->name, ";");
    h.println("};");
    for (const auto& alias : struct_->aliases) {
      h.println("using ", alias, " = ", struct_->name, ";");
      h.println();
    }
    h.println();
    if (struct_->struct_->platform) h.println("#endif");
  }

  h.println();
  h.println("}  // namespace spk");
}

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  CHECK(!FLAGS_vkxml.empty()) << "--vkxml required";

  tinyxml2::XMLDocument doc;
  CHECK(doc.LoadFile(FLAGS_vkxml.c_str()) == tinyxml2::XML_SUCCESS)
      << "Unable to parse " << FLAGS_vkxml;

  auto start = relaxng::parse<vkr::start>(doc.RootElement());

  if (!FLAGS_outjson.empty()) {
    dvc::file_writer fw(FLAGS_outjson, dvc::truncate);
    dvc::json_writer jw(fw.ostream());
    write_json(jw, start);
  }

  vks::Registry vksregistry = parse_registry(start);

  if (!FLAGS_outtest.empty()) write_test(vksregistry);

  sps::Registry spsregistry = build_spock_registry(vksregistry);

  if (!FLAGS_outh.empty()) write_header(spsregistry);
}
