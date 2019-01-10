#include <gflags/gflags.h>
#include <glog/logging.h>
#include <iostream>
#include <set>
#include <unordered_set>

#include "dvc/container.h"
#include "dvc/file.h"
#include "dvc/string.h"

#include "sps/sps.h"
#include "sps/spsbuilder.h"
#include "vks/vks.h"
#include "vks/vksparser.h"
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
  h.println("#include <array>");
  h.println("#include <cstring>");
  h.println("#include <type_traits>");
  h.println("#include <vulkan/vulkan.h>");
  h.println();
  h.println("#include \"spk/spock_fwd.h\"");
  h.println();
  h.println("namespace spk {");
  h.println();

  for (const sps::Bitmask* bitmask : registry.bitmasks) {
    if (bitmask->enumerators.empty()) continue;
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
      h.println("SPK_DEFINE_BITMASK_BITWISE_OPS(", name, ");");
      h.println();
    }
    for (const auto& alias : bitmask->aliases) {
      h.println("using ", alias, " = ", bitmask->name, ";");
      h.println();
    }
  }

  for (const sps::Enumeration* enumeration : registry.enumerations) {
    if (enumeration->enumerators.empty()) continue;
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

  h.println("// handle refs");
  for (const auto& handle : registry.handles) {
    h.println("using ", handle->name, " = ", handle->handle->name, ";");
  }

  h.println("// struct fwd decls");
  for (const auto& struct_ : registry.structs) {
    if (struct_->struct_->platform)
      h.println("#ifdef ", struct_->struct_->platform->protect);
    std::string keyword = (struct_->struct_->is_union ? "union" : "class");
    h.println(keyword, " ", struct_->name, ";");
    if (struct_->struct_->platform) h.println("#endif");
  }
  h.println();
  for (const auto& struct_ : registry.structs) {
    if (struct_->struct_->platform)
      h.println("#ifdef ", struct_->struct_->platform->protect);
    std::string keyword = (struct_->struct_->is_union ? "union" : "class");
    h.println(keyword, " ", struct_->name, " {");
    h.println(" public:");
    h.println("  using underlying_type = ", struct_->struct_->name, ";");
    h.println();
    if (struct_->struct_->is_union) {
      h.println("  ", struct_->name, "() { std::memset(this, 0, sizeof(",
                struct_->name, ")); }");
      h.println();
    }
    //    auto member_name = [&](size_t member_idx) {
    //      return struct_->struct_->members.at(member_idx).name + "_";
    //    };

    for (bool mutator : {true, false}) {
      if (mutator) {
        h.println("  // mutators");
      } else {
        h.println("  // accessors");
      }

      for (const auto& accessor : struct_->accessors) {
        if (auto value_accessor =
                dynamic_cast<const sps::ValueAccessor*>(accessor)) {
          CHECK(dvc::endswith(value_accessor->member->name, "_"));
          std::string mname = value_accessor->member->name;
          std::string bname = mname.substr(0, mname.size() - 1);
          bool large = value_accessor->large;

          if (mutator) {
            h.println("  void set_", bname, "(", (large ? "const " : ""),
                      value_accessor->member->stype->make_declaration(
                          (large ? "& value" : "value"), false),
                      ") { ", mname, " = value; }");
          } else {
            h.println("  ", (large ? "const " : ""),
                      value_accessor->member->stype->make_declaration(
                          (large ? "& " : "") + bname, false),
                      "() const { return ", mname, "; }");
          }
        } else if (auto string_accessor =
                       dynamic_cast<const sps::StringAccessor*>(accessor)) {
          std::string mname = string_accessor->member->name;
          std::string aname = string_accessor->name;

          if (mutator) {
            h.println("  void set_", aname, "(spk::string_ptr str) { ", mname,
                      " = str.get(); }");
          } else {
            h.println("  std::string_view ", aname, "() { return ", mname,
                      "; }");
          }
        } else if (auto bool_accessor =
                       dynamic_cast<const sps::BoolAccessor*>(accessor)) {
          std::string mname = bool_accessor->member->name;
          std::string aname = bool_accessor->name;

          if (mutator) {
            h.println("  void set_", aname, "(bool val) { ", mname,
                      " = val; }");
          } else {
            h.println("  bool ", aname, "() { return ", mname, "; }");
          }
        } else if (auto span_accessor =
                       dynamic_cast<const sps::SpanAccessor*>(accessor)) {
          std::string mcount = span_accessor->count->name;
          std::string msubject = span_accessor->subject->name;
          const vks::Pointer* ptr_type =
              dynamic_cast<const vks::Pointer*>(span_accessor->subject->stype);
          CHECK(ptr_type);
          const vks::Type* element_type = ptr_type->T;
          std::string aname = span_accessor->name;

          if (mutator) {
            h.println("  void set_", aname, "(spk::array_view<",
                      element_type->to_string(), "> value) { ", msubject,
                      " = value.get(); ", mcount, " = value.size(); }");
          } else {
            h.println("  spk::array_view<", element_type->to_string(), "> ",
                      aname, "() { return {", msubject, ", ", mcount, "};}");
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
    for (const auto& member : struct_->members)
      if (member.empty_enum())
        h.println("  VkFlags ", member.name, "; // reserved");
      else
        h.println("  ",
                  member.stype->make_declaration(
                      member.name, /*zero*/ !struct_->struct_->is_union),
                  ";", (member.optional(0) ? "// optional" : ""));
    h.println("};");
    h.println("static_assert(sizeof(", struct_->name, ") == sizeof(",
              struct_->name, "::underlying_type));");
    for (const auto& alias : struct_->aliases) {
      h.println("using ", alias, " = ", struct_->name, ";");
      h.println();
    }
    if (struct_->struct_->platform) h.println("#endif");
    h.println();
  }

  for (vks::DispatchTableKind kind :
       {vks::DispatchTableKind::GLOBAL, vks::DispatchTableKind::INSTANCE,
        vks::DispatchTableKind::DEVICE}) {
    std::string name = std::string(vks::to_string(kind)) + "_dispatch_table";
    const vks::DispatchTable* dispatch_table =
        registry.vreg->dispatch_table(kind);
    h.println("struct ", name, " {");
    for (const vks::Command* command : dispatch_table->commands) {
      if (command->platform) h.println("#ifdef ", command->platform->protect);
      h.println("  PFN_", command->name, " ", command->name, ";");
      if (command->platform) h.println("#endif");
    }
    h.println("};");
  }

  for (const auto& handle : registry.handles) {
    std::string sname = handle->fullname;
    std::string rname = handle->name;
    std::string vname = handle->handle->name;

    h.println("class ", sname, " {");
    h.println(" public:");
    h.println("  operator ", rname, "() const { return handle_; }");
    h.println();
    for (const auto& command : handle->commands) {
      if (command->command->platform)
        h.println("#ifdef ", command->command->platform->protect);
      h.println("  void ", command->name, "();");
      if (command->command->platform) h.println("#endif");
    }
    h.println();
    h.println(" private:");
    h.println("  ", rname, " handle_ = VK_NULL_HANDLE;");
    h.println();
    h.println("};");
    for (const auto& alias : handle->aliases) {
      h.println("using ", alias, " = ", handle->name, ";");
      h.println();
    }
    h.println();
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
