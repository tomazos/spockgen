#include <gflags/gflags.h>
#include <glog/logging.h>
#include <iostream>
#include <set>
#include <unordered_set>

#include "dvc/container.h"
#include "dvc/file.h"
#include "dvc/string.h"

#include "sps/spock_api_schema.h"
#include "sps/spock_api_schema_builder.h"
#include "vks/vulkan_api_schema.h"
#include "vks/vulkan_api_schema_parser.h"
#include "vks/vulkan_relaxng.h"

DEFINE_string(vkxml, "", "Input vk.xml file");
DEFINE_string(outjson, "", "Output AST to json");
DEFINE_string(outtest, "", "Output test of API");
DEFINE_string(outh, "", "Output C++ header");

void write_test(const vks::Registry& registry) {
  dvc::file_writer test(FLAGS_outtest, dvc::truncate);

  test.println("#include \"test/vkxmltest.h\"");

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
  h.println("#include <type_traits>");
  h.println("#include <vulkan/vulkan.h>");
  h.println();
  h.println("namespace spk {");
  h.println();
  h.println("static_assert(std::is_same_v<VkDeviceSize, uint64_t>);");
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
    std::string keyword = (struct_->struct_->is_union ? "union" : "struct");
    h.println(keyword, " ", struct_->name, " {");
    auto member_name = [&](size_t member_idx) {
      return struct_->struct_->members.at(member_idx).name + "_";
    };
    h.println("  using underlying_type = ", struct_->struct_->name, ";");
    h.println();

    for (bool mutator : {true, false}) {
      if (mutator) {
        h.println("  // mutators");
      } else {
        h.println("  // accessors");
      }

      for (const auto& accessor : struct_->accessors) {
        if (auto singular_bool =
                dynamic_cast<const sps::SingularBool*>(accessor)) {
          std::string aname = singular_bool->name;
          std::string mname = member_name(singular_bool->member_idx);
          if (mutator) {
            h.println("  void set_", aname, "(bool value) { ", mname,
                      " = VkBool32(value); }");

          } else {
            h.println("  bool ", aname, "() const { return bool(", mname,
                      ");}");
          }
        } else if (auto singular_numeric =
                       dynamic_cast<const sps::SingularNumeric*>(accessor)) {
          std::string aname = singular_numeric->name;
          std::string mname = member_name(singular_numeric->member_idx);
          std::string type = singular_numeric->type;
          if (mutator) {
            h.println("  void set_", aname, "(", type, " value) { ", mname,
                      " = value; }");
          } else {
            h.println("  ", type, " ", aname, "() const { return ", mname,
                      ";}");
          }
        } else if (auto singular_enumeration =
                       dynamic_cast<const sps::SingularEnumeration*>(
                           accessor)) {
          std::string aname = singular_enumeration->name;
          std::string mname = member_name(singular_enumeration->member_idx);
          std::string atype = "spk::" + singular_enumeration->enumeration->name;
          std::string mtype =
              singular_enumeration->enumeration->enumeration->name;

          if (mutator) {
            h.println("  void set_", aname, "(", atype, " value) { ", mname,
                      " = ", mtype, "(value); }");
          } else {
            h.println("  ", atype, " ", aname, "() const { return ", atype, "(",
                      mname, ");}");
          }
        } else if (auto singular_bitmask =
                       dynamic_cast<const sps::SingularBitmask*>(accessor)) {
          std::string aname = singular_bitmask->name;
          std::string mname = member_name(singular_bitmask->member_idx);
          std::string atype = "spk::" + singular_bitmask->bitmask->name;
          std::string mtype =
              (singular_bitmask->flag_bits
                   ? struct_->struct_->members.at(singular_bitmask->member_idx)
                         .type->to_string()
                   : singular_bitmask->bitmask->bitmask->name);

          if (mutator) {
            h.println("  void set_", aname, "(", atype, " value) { ", mname,
                      " = ", mtype, "(value); }");
          } else {
            h.println("  ", atype, " ", aname, "() const { return ", atype, "(",
                      mname, ");}");
          }
        } else {
          LOG(FATAL) << "Unknown Accessor subclass " << typeid(accessor).name();
        }
      }
      h.println();
    }
    h.println(" private:");
    if (struct_->struct_->structured_type) {
      h.println("  VkStructureType s_type_ = ",
                struct_->struct_->structured_type->name, ";");
      h.println("  void* p_next_ = nullptr;");
      h.println();
    }
    for (const auto& member : struct_->struct_->members)
      h.println("  ", member.type->make_declaration(member.name + "_"), ";",
                (member.len ? "// len=" + member.len.value() : ""));
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

  if (!FLAGS_outh.empty()) {
    sps::Registry spsregistry = build_spock_registry(vksregistry);

    write_header(spsregistry);
  }
}
