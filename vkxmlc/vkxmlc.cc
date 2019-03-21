#include <iostream>
#include <set>
#include <unordered_set>

#include "dvc/container.h"
#include "dvc/file.h"
#include "dvc/opts.h"
#include "dvc/string.h"

#include "sps/sps.h"
#include "sps/spsbuilder.h"
#include "vks/vks.h"
#include "vks/vksparser.h"
#include "vks/vulkan_relaxng.h"

std::string DVC_OPTION(vkxml, -, "", "Input vk.xml file");
std::string DVC_OPTION(outjson, -, "", "Output AST to json");
std::string DVC_OPTION(outtest, -, "", "Output test of API");
std::string DVC_OPTION(outh, -, "", "Output C++ header");

void write_test(const vks::Registry& registry) {
  dvc::file_writer test(outtest, dvc::truncate);

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
    if (bitmask->requires_)
      test.println("VKXMLTEST_CHECK_BITMASK_REQUIRES(", name, ", ",
                   bitmask->requires_->name, ");");
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
  dvc::file_writer h(outh, dvc::truncate);

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
    h.println();
    for (const auto& enumerator : bitmask->enumerators)
      h.println("  ", enumerator.name, " = ", enumerator.constant->name, ",");
    h.println("};");
    h.println("SPK_DEFINE_BITMASK_BITWISE_OPS(", name, ");");
    h.println();
    for (const auto& alias : bitmask->aliases) {
      h.println("using ", alias, " = ", bitmask->name, ";");
      h.println();
    }
    h.println("inline std::ostream& operator<<(std::ostream& o, ", name,
              " e){");
    h.println("  SPK_BEGIN_BITMASK_OUTPUT(", name, ")");
    for (const auto& enumerator : bitmask->enumerators) {
      h.println("  SPK_BITMASK_OUTPUT_ENUMERATOR(", name, ", ", enumerator.name,
                ")");
    }
    h.println("  SPK_END_BITMASK_OUTPUT(", name, ")");
    h.println("}");
  }

  for (const sps::Enumeration* enumeration : registry.enumerations) {
    if (enumeration->enumerators.empty()) continue;
    std::string name = enumeration->name;
    h.println("// enumeration ", enumeration->enumeration->name);
    h.println("enum class ", name, " {");
    for (const auto& enumerator : enumeration->enumerators)
      h.println("  ", enumerator.name, " = ", enumerator.constant->name, ",");
    h.println("};");
    h.println();
    for (const auto& alias : enumeration->aliases) {
      h.println("using ", alias, " = ", enumeration->name, ";");
      h.println();
    }
    h.println("inline std::ostream& operator<<(std::ostream& o, ", name,
              " e){");
    h.println("  SPK_BEGIN_ENUMERATION_OUTPUT(", name, ")");
    for (const auto& enumerator : enumeration->enumerators) {
      h.println("  SPK_ENUMERATION_OUTPUT_ENUMERATOR(", name, ", ",
                enumerator.name, ")");
    }
    h.println("  SPK_END_ENUMERATION_OUTPUT(", name, ")");
    h.println("}");
    if (enumeration->name == "result") {
      h.println("SPK_BEGIN_RESULT_ERRORS");
      for (const auto& enumerator : enumeration->enumerators)
        if (dvc::startswith(enumerator.name, "error"))
          h.println("SPK_DEFINE_RESULT_ERROR(", enumerator.name, ")");
      h.println("SPK_END_RESULT_ERRORS");
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
          DVC_ASSERT(dvc::endswith(value_accessor->member->name, "_"));
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
            h.println("  std::string_view ", aname, "() const { return ", mname,
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
            h.println("  bool ", aname, "() const { return ", mname, "; }");
          }
        } else if (auto span_accessor =
                       dynamic_cast<const sps::SpanAccessor*>(accessor)) {
          std::string mcount = span_accessor->count->name;
          std::string msubject = span_accessor->subject->name;
          const vks::Pointer* ptr_type =
              dynamic_cast<const vks::Pointer*>(span_accessor->subject->stype);
          DVC_ASSERT(ptr_type);
          const vks::Type* element_type = ptr_type->T;
          std::string aname = span_accessor->name;

          if (mutator) {
            h.println("  void set_", aname, "(spk::array_ptr<",
                      element_type->to_string(), "> value) { ", msubject,
                      " = value.data(); ", mcount, " = value.size(); }");
          } else {
            h.println("  spk::array_view<", element_type->to_string(), "> ",
                      aname, "() const { return {", msubject, ", ", mcount,
                      "};}");
          }
        } else {
          DVC_FATAL("Unknown Accessor subclass ", typeid(accessor).name());
        }
      }
      h.println();
    }
    if (struct_->struct_->structured_type) {
      h.println("  void set_next(void* next) { p_next_ = next; }");
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
        h.println("  VkFlags ", member.name, " = {}; // reserved");
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
    const sps::DispatchTable* dispatch_table = registry.dispatch_table(kind);
    h.println("struct ", name, " {");
    if (kind == vks::DispatchTableKind::INSTANCE)
      h.println("  spk::instance_ref instance;");
    else if (kind == vks::DispatchTableKind::DEVICE)
      h.println("  spk::device_ref device;");

    h.println("  // spock commands");
    for (const sps::Command* command : dispatch_table->commands) {
      bool isresult = (command->vreturn_type->to_string() == "VkResult");
      bool ismultisuccess =
          (isresult && command->command->successcodes.size() > 1);
      bool isonesuccess = (isresult && !ismultisuccess);
      bool isvoid = command->vreturn_type->to_string() == "void";
      bool isother = (!isvoid && !isresult);

      if (command->command->platform)
        h.println("#ifdef ", command->command->platform->protect);

      if (ismultisuccess) {
        h.println("  [[nodiscard]] spk::result ", command->name, "(");
      } else {
        h.println("  ", command->sreturn_type->to_string(), " ", command->name,
                  "(");
      }
      bool first = true;
      for (const sps::Param& param : command->params) {
        if (param.stype->is_empty_enum()) continue;
        if (first) {
          first = false;
        } else {
          h.println(",");
        }
        h.print("    ", param.stype->make_declaration(param.name, 0));
      }
      h.println();
      h.println("  ) const {");

      if (isresult) h.println("    const VkResult res = ");
      if (isother)
        h.println("    return (", command->sreturn_type->to_string(), ")(");
      h.println("    ", command->command->name, "(");
      first = true;
      for (const sps::Param& param : command->params) {
        if (first) {
          first = false;
        } else {
          h.println(",");
        }
        if (param.stype->is_empty_enum())
          h.print("      (", param.vtype->to_string(), ") 0 /*", param.name,
                  "*/");
        else if (dynamic_cast<const vks::Array*>(param.vtype))
          h.print("      ", param.name, ".data()");
        else
          h.print("      (", param.vtype->to_string(), ") ", param.name);
      }
      h.println();
      h.print("    )");
      if (isother) h.print(")");
      h.println(";");
      if (isresult) {
        h.println("    switch (res) {");
        if (isonesuccess) {
          DVC_ASSERT(command->command->successcodes.at(0)->name ==
                     "VK_SUCCESS");
          h.println("      case VK_SUCCESS: return;");
        } else {
          for (const sps::Enumerator* successcode : command->successcodes) {
            h.println("      case ", successcode->constant->name,
                      ": return spk::result::", successcode->name, ";");
          }
        }

        for (const sps::Enumerator* errorcode : command->errorcodes) {
          h.println("      case ", errorcode->constant->name,
                    ": throw spk::", errorcode->name, "();");
        }
        h.println(
            "      default: throw spk::unexpected_command_result((spk::result) "
            "res, \"",
            command->command->name, "\");");
        h.println("    }");
      }
      h.println("  }");

      if (command->command->platform) h.println("#endif");
      h.println();
    }

    h.println("  // vulkan commands");
    for (const vks::Command* command :
         dispatch_table->dispatch_table->commands) {
      if (command->platform) h.println("#ifdef ", command->platform->protect);
      h.println("  PFN_", command->name, " ", command->name, " = nullptr;");
      if (command->platform) h.println("#endif");
    }
    h.println("};");
    h.println();
    h.println("template<class Visitor> void visit_dispatch_table(", name,
              "& dispatch_table, const Visitor& V) {");
    for (const vks::Command* command :
         dispatch_table->dispatch_table->commands) {
      if (command->platform) h.println("#ifdef ", command->platform->protect);
      std::string c = command->name;
      h.println("  V(dispatch_table, &", name, "::", c, ", \"", c, "\");");
      if (command->platform) h.println("#endif");
    }
    h.println("}");
    h.println();
  }

  h.println(R"(

inline std::unique_ptr<spk::global_dispatch_table> load_global_dispatch_table(
    PFN_vkGetInstanceProcAddr pvkGetInstanceProcAddr) {
  auto global_dispatch_table = std::make_unique<spk::global_dispatch_table>();

  spk::visit_dispatch_table(
      *global_dispatch_table,
      [pvkGetInstanceProcAddr](spk::global_dispatch_table& t, auto mf,
                               const char* name) {
        using PFN = strip_member_function_t<decltype(mf)>;
        (t.*mf) = (PFN)pvkGetInstanceProcAddr(VK_NULL_HANDLE, name);
      });

  return global_dispatch_table;
}

inline std::unique_ptr<spk::instance_dispatch_table> load_instance_dispatch_table(
    PFN_vkGetInstanceProcAddr pvkGetInstanceProcAddr,
    spk::instance_ref instance) {
  auto instance_dispatch_table = std::make_unique<spk::instance_dispatch_table>();

  instance_dispatch_table->instance = instance;

  spk::visit_dispatch_table(
      *instance_dispatch_table,
      [pvkGetInstanceProcAddr, instance](spk::instance_dispatch_table& t,
                                         auto mf, const char* name) {
        using PFN = spk::strip_member_function_t<decltype(mf)>;
        (t.*mf) = (PFN)pvkGetInstanceProcAddr(instance, name);
      });

  return instance_dispatch_table;
}

inline std::unique_ptr<spk::device_dispatch_table> load_device_dispatch_table(
    PFN_vkGetDeviceProcAddr pvkGetDeviceProcAddr, spk::device_ref device) {
  auto device_dispatch_table = std::make_unique<spk::device_dispatch_table>();

  device_dispatch_table->device = device;

  spk::visit_dispatch_table(
      *device_dispatch_table,
      [pvkGetDeviceProcAddr, device](spk::device_dispatch_table& t, auto mf,
                                     const char* name) {
        using PFN = spk::strip_member_function_t<decltype(mf)>;
        (t.*mf) = (PFN)pvkGetDeviceProcAddr(device, name);
      });

  return device_dispatch_table;
}

)");

  static std::set<std::string> instance_handles = {"instance",
                                                   "physical_device",
                                                   "debug_report_callback_ext",
                                                   "debug_utils_messenger_ext",
                                                   "display_mode_khr",
                                                   "surface_khr",
                                                   "display_khr"};

  h.println("// fwd declare loader");
  h.println("class loader;");
  h.println();

  h.println("// fwd declare handles");
  for (const auto& handle : registry.handles) {
    std::string sname = handle->fullname;
    h.println("class ", sname, ";");
  }

  h.println();
  h.println("// handles");
  for (const auto& handle : registry.handles) {
    std::string sname = handle->fullname;
    std::string rname = handle->name;
    std::string vname = handle->handle->name;

    h.println("class ", sname, " ");
    h.println("{");

    h.println(" public:");
    h.println("  operator ", rname, "() const { return handle_; }");

    if (sname == "instance") {
      h.println(
          "  instance(spk::instance_ref handle, const spk::loader& loader, "
          "spk::allocation_callbacks const* allocation_callbacks "
          "= "
          "nullptr);");
    } else if (sname == "device") {
      h.println(
          "device(spk::device_ref handle, spk::physical_device& "
          "physical_device,"
          "spk::allocation_callbacks const* allocation_callbacks);");
    } else {
      h.print("  ", sname, "(", rname, " handle");
      if (handle->parent) h.print(", ", handle->parent->name, " parent");

      if (instance_handles.count(sname))
        h.print(", const instance_dispatch_table& dispatch_table");
      else
        h.print(", const device_dispatch_table& dispatch_table");
      h.println(", const spk::allocation_callbacks* allocation_callbacks)");
      h.println("  : handle_(handle)");
      if (handle->parent) h.println("  , parent_(parent)");
      h.println("  , dispatch_table_(&dispatch_table)");
      h.println("  , allocation_callbacks_(allocation_callbacks) {}");
    }
    h.println("  ", sname, "(const ", sname, "&) = delete;");
    h.println("  ", sname, "(", sname, "&& that)");
    h.println("  : handle_(that.handle_)");
    if (handle->parent) h.println("  , parent_(that.parent_)");
    h.println("  , dispatch_table_(std::move(that.dispatch_table_))");
    h.println(
        "  , allocation_callbacks_(std::move(that.allocation_callbacks_)) { "
        "that.handle_ "
        "= VK_NULL_HANDLE; }");
    h.println();
    h.println("void release() { handle_ = VK_NULL_HANDLE; }");
    h.println();
    for (auto member_function : handle->member_functions) {
      if (member_function->command->command->platform)
        h.println(" #ifdef ",
                  member_function->command->command->platform->protect);
      h.println("  // ", member_function->command->command->name);
      if (member_function->manual_translation) {
        h.print(member_function->manual_translation->interface);
      } else {
        h.print("  inline ");
        std::string rtype;
        if (member_function->result_handle)
          rtype = "spk::" + member_function->result_handle->fullname;
        else if (member_function->res)
          rtype = member_function->res->to_string();

        if (member_function->result) {
          h.print(rtype);
        } else if (member_function->resultvec_void ||
                   member_function->resultvec_incomplete) {
          h.print("std::vector<", rtype, ">");
        } else {
          h.print(member_function->command->sreturn_type->to_string());
        }
        h.print(" ", member_function->name, "(");
        bool first = true;
        for (size_t i = member_function->begin(); i < member_function->end();
             i++) {
          const sps::Param& param = member_function->command->params.at(i);
          if (param.stype->is_empty_enum() || param.is_allocation_callbacks())
            continue;
          if (first)
            first = false;
          else
            h.print(", ");
          if (member_function->szptrs.count(i)) {
            const sps::Param& param =
                member_function->command->params.at(i + 1);
            h.print("spk::array_view<",
                    sps::get_pointee(param.stype)->to_string(), "> ",
                    param.name);
            i++;
          } else if (auto ref = param.asref()) {
            h.print(ref->make_declaration("&" + param.name, 0));
          } else {
            h.print(param.stype->make_declaration(param.name, 0));
          }
        }
        h.println(");");
      }
      if (member_function->command->command->platform) h.println("#endif");
      h.println();
    }

    h.println();
    if (instance_handles.count(sname))
      h.println(
          "  const instance_dispatch_table& dispatch_table() { return "
          "*dispatch_table_; }");
    else
      h.println(
          "  const device_dispatch_table& dispatch_table() { return "
          "*dispatch_table_; }");

    if (handle->destructor) {
      h.print("  ~", sname,
              "() { if (handle_ != VK_NULL_HANDLE) dispatch_table().",
              handle->destructor->name, "(");
      if (handle->destructor_parent)
        h.print("parent_, handle_, allocation_callbacks_);");
      else
        h.print("handle_, allocation_callbacks_);");
      h.println("}");
    }

    h.println(" private:");
    h.println("  ", rname, " handle_ = VK_NULL_HANDLE;");
    if (handle->parent)
      h.println("  ", handle->parent->name, " parent_ = VK_NULL_HANDLE;");
    if (sname == "instance")
      h.println(
          "  std::unique_ptr<const instance_dispatch_table> dispatch_table_;");
    else if (instance_handles.count(sname))
      h.println("  const instance_dispatch_table* dispatch_table_;");
    else if (sname == "device")
      h.println(
          "  std::unique_ptr<const device_dispatch_table> dispatch_table_;");
    else
      h.println("  const device_dispatch_table* dispatch_table_;");
    h.println(
        "  const spk::allocation_callbacks* allocation_callbacks_ = nullptr;");
    //    if (sname == "physical_device") h.println("\n  friend class device;");
    h.println("};");
    for (const auto& alias : handle->aliases) {
      h.println("using ", alias, " = ", handle->name, ";");
      h.println();
    }
    h.println();
  }

  for (const auto& handle : registry.handles) {
    std::string sname = handle->fullname;
    std::string rname = handle->name;

    for (auto member_function : handle->member_functions) {
      if (member_function->command->command->platform)
        h.println("#ifdef ",
                  member_function->command->command->platform->protect);
      if (member_function->manual_translation) {
        h.print(member_function->manual_translation->implementation);
      } else {
        h.print("inline ");
        std::string rtype;
        if (member_function->result_handle)
          rtype = "spk::" + member_function->result_handle->fullname;
        else if (member_function->res)
          rtype = member_function->res->to_string();

        if (member_function->result) {
          h.print(rtype);
        } else if (member_function->resultvec_void ||
                   member_function->resultvec_incomplete) {
          h.print("std::vector<", rtype, ">");
        } else {
          h.print(member_function->command->sreturn_type->to_string());
        }
        h.print(" ", sname, "::", member_function->name, "(");
        bool first = true;
        for (size_t i = member_function->begin(); i < member_function->end();
             i++) {
          const sps::Param& param = member_function->command->params.at(i);
          if (param.stype->is_empty_enum() || param.is_allocation_callbacks())
            continue;
          if (first)
            first = false;
          else
            h.print(", ");
          if (member_function->szptrs.count(i)) {
            const sps::Param& param =
                member_function->command->params.at(i + 1);
            h.print("spk::array_view<",
                    sps::get_pointee(param.stype)->to_string(), "> ",
                    param.name);
            i++;
          } else if (auto ref = param.asref()) {
            h.print(ref->make_declaration("&" + param.name, 0));
          } else {
            h.print(param.stype->make_declaration(param.name, 0));
          }
        }
        h.println(") {");
        auto write_call = [&] {
          h.print("  dispatch_table().", member_function->command->name, "(");
          if (member_function->parent_dispatch)
            h.print("parent_, handle_");
          else
            h.print("handle_");
          for (size_t i = member_function->begin(); i < member_function->end();
               i++) {
            const sps::Param& param = member_function->command->params.at(i);
            if (param.stype->is_empty_enum()) continue;
            h.print(", ");
            if (param.is_allocation_callbacks())
              h.print("allocation_callbacks_");
            else if (member_function->szptrs.count(i)) {
              h.print(member_function->command->params.at(i + 1).name,
                      ".size()");
            } else if (member_function->szptrs.count(i - 1)) {
              h.print(param.name, ".data()");
            } else if (param.asref()) {
              h.print("&", param.name);
            } else {
              h.print(param.name);
            }
          }
        };
        if (member_function->result) {
          h.println("  ", member_function->res->to_string(), " result_;");
          write_call();
          h.println(", &result_);");

          if (!member_function->result_handle) {
            h.println("  return result_;");
          } else {
            const sps::Handle* reshand = member_function->result_handle;
            h.print("  return {result_");
            if (reshand->parent) {
              if (reshand->parent != handle) {
                DVC_ERROR("mismatch parent: ", member_function->command->name,
                          " member of ", handle->name, " but parent ",
                          reshand->parent->name);
              }
              h.print(", handle_");
            }
            h.println(", dispatch_table(), allocation_callbacks_};");
          }
        } else if (member_function->resultvec_void ||
                   member_function->resultvec_incomplete) {
          h.println("  ", member_function->sz->to_string(), " size_;");
          for (int pass : {1, 2}) {
            if (member_function->resultvec_incomplete) {
              h.print("  spk::result success", pass, "_ = ");
            }
            write_call();
            if (pass == 1)
              h.println(", &size_, nullptr);");
            else if (pass == 2)
              h.println(", &size_, result_.data());");
            if (member_function->resultvec_incomplete) {
              h.println("  if (success", pass,
                        "_ != spk::result::success) throw "
                        "spk::unexpected_command_result(success",
                        pass, "_, \"", member_function->command->command->name,
                        "\");");
            }
            if (pass == 1)
              h.println("  std::vector<", member_function->res->to_string(),
                        "> result_(size_);");
          }
          if (!member_function->result_handle) {
            h.println("  return result_;");
          } else {
            const sps::Handle* reshand = member_function->result_handle;
            h.println("  std::vector<", rtype, "> result2_;");
            h.println("  result2_.reserve(size_);");
            h.println("  for (auto ref : result_)");
            h.println("    result2_.emplace_back(ref");
            if (reshand->parent) {
              if (reshand->parent != handle) {
                DVC_ERROR("mismatch parent: ", member_function->command->name,
                          " member of ", handle->name, " but parent ",
                          reshand->parent->name);
              }
              h.print(", handle_");
            }
            h.println(", dispatch_table(), allocation_callbacks_);");
            h.println("  return result2_;");
          }
        } else {
          if (member_function->command->sreturn_type->to_string() != "void")
            h.print("  return ");
          write_call();
          h.println(");");
        }
        h.println("}");
      }
      if (member_function->command->command->platform) h.println("#endif");
      h.println();
    }

    h.println();
  }
  h.println();
  h.println("}  // namespace spk");
}

int main(int argc, char** argv) {
  dvc::init_options(argc, argv);

  DVC_ASSERT(!vkxml.empty(), "--vkxml required");

  tinyxml2::XMLDocument doc;
  DVC_ASSERT(doc.LoadFile(vkxml.c_str()) == tinyxml2::XML_SUCCESS,
             "Unable to parse ", vkxml);

  auto start = relaxng::parse<vkr::start>(doc.RootElement());

  if (!outjson.empty()) {
    dvc::file_writer fw(outjson, dvc::truncate);
    dvc::json_writer jw(fw.ostream());
    write_json(jw, start);
  }

  vks::Registry vksregistry = parse_registry(start);

  if (!outtest.empty()) write_test(vksregistry);

  if (!outh.empty()) {
    sps::Registry spsregistry = build_spock_registry(vksregistry);

    write_header(spsregistry);
    DVC_ASSERT_EQ(
        0, std::system(
               ("/usr/bin/clang-format -i -style=Google " + outh).c_str()));
  }
}
