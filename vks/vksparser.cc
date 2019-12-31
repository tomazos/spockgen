#include "vks/vksparser.h"

#include <map>
#include <set>

#include "dvc/container.h"
#include "dvc/string.h"
#include "mnc/minic_parser.h"

namespace {

void parse_platforms(vks::Registry& registry, const vkr::start& start) {
  for (const auto& platforms : start.platforms)
    for (const vkr::Platform& platform_in : platforms.platform) {
      auto platform_out = new vks::Platform;
      platform_out->name = platform_in.name;
      platform_out->protect = platform_in.protect;
      dvc::insert_or_die(registry.platforms, platform_out->name, platform_out);
    }
}

std::string enum_to_value(const vkr::Enum& enum_,
                          std::optional<std::string> extnumber = std::nullopt) {
  if (enum_.value)
    return "(" + enum_.value.value() + ")";
  else if (enum_.bitpos)
    return "(1 << (" + enum_.bitpos.value() + "))";
  else if (enum_.alias)
    return "(" + enum_.alias.value() + ")";
  else if (enum_.offset) {
    if (enum_.extnumber) extnumber = enum_.extnumber;
    DVC_ASSERT(extnumber);
    bool neg = enum_.dir.has_value();
    if (neg) DVC_ASSERT(enum_.dir.value() == "-");
    return std::string("(") + (neg ? "-1" : "+1") + "* (1'000'000'000 + (" +
           extnumber.value() + "-1) * 1'000 + " + enum_.offset.value() + "))";
  } else {
    DVC_FATAL("bad enum ", enum_.name);
  }
}

template <typename F>
void foreach_extension(const vks::Registry& registry, const vkr::start& start,
                       F process_require) {
  for (const vkr::Extensions& extensions : start.extensions) {
    for (const vkr::Extension& extension : extensions.extension) {
      std::string supported = extension.supported.value();
      DVC_ASSERT(supported == "disabled" || supported == "vulkan", supported);
      if (supported == "disabled") continue;

      std::optional<std::string> extnumber = extension.number;

      const vks::Platform* platform = nullptr;
      if (extension.platform)
        platform = registry.platforms.at(extension.platform.value());

      DVC_ASSERT(extension.remove.empty());
      for (const vkr::Extension_require& require : extension.require)
        process_require(require, extnumber, platform);
    }
  }
}

void parse_externals(vks::Registry& registry, const vkr::start& start) {
  for (const vkr::Types& stypes : start.types)
    for (const vkr::Type& type : stypes.type) {
      if (!type.category.has_value() || type.category == "basetype" ||
          type.category == "define") {
        std::string name = type.name_attribute.has_value()
                               ? type.name_attribute.value()
                               : type.name_subelement.value();
        auto external = new vks::External;
        external->name = name;
        dvc::insert_or_die(registry.externals, name, external);
      }
    }
}

void parse_constants(vks::Registry& registry,
                     std::multimap<std::string, vks::Constant*>& extends,
                     const vkr::start& start) {
  for (const vkr::Enums& enums : start.enums)
    for (const vkr::Enum& enum_ : enums.enum_) {
      auto constant = new vks::Constant;
      constant->name = enum_.name;
      constant->value = enum_to_value(enum_);
      dvc::insert_or_die(registry.constants, enum_.name, constant);
      if (enum_.extends)
        extends.insert(std::make_pair(enum_.extends.value(), constant));
    }

  for (const vkr::Feature& feature : start.feature) {
    DVC_ASSERT(feature.remove.empty());
    for (const auto& require : feature.require) {
      for (const vkr::Enum& enum_ : require.enum_) {
        bool extension_enum =
            enum_.value || enum_.bitpos || enum_.alias || enum_.offset;
        if (!extension_enum)
          DVC_ASSERT(registry.constants.count(enum_.name) == 1, enum_.name);
        else {
          auto constant = new vks::Constant;
          constant->name = enum_.name;
          constant->value = enum_to_value(enum_);
          dvc::insert_or_die(registry.constants, enum_.name, constant);
          if (enum_.extends)
            extends.insert(std::make_pair(enum_.extends.value(), constant));
        }
      }
    }
  }

  foreach_extension(
      registry, start, [&](auto require, auto extnumber, auto platform) {
        for (const vkr::Enum& enum_ : require.enum_) {
          bool extension_enum =
              enum_.value || enum_.bitpos || enum_.alias || enum_.offset;
          if (!extension_enum)
            DVC_ASSERT(registry.constants.count(enum_.name) == 1, enum_.name);
          else {
            std::string name = enum_.name;
            std::string value = enum_to_value(enum_, extnumber);
            if (registry.constants.count(name))
              DVC_ASSERT_EQ(value, registry.constants.at(name)->value,
                            "mismatched value of ", name);
            else {
              auto constant = new vks::Constant;
              constant->name = enum_.name;
              constant->value = value;
              constant->platform = platform;

              dvc::insert_or_die(registry.constants, enum_.name, constant);
              if (enum_.extends)
                extends.insert(std::make_pair(enum_.extends.value(), constant));
            }
          }
        }
      });
}

void parse_enumerations(vks::Registry& registry, const vkr::start& start) {
  std::unordered_map<std::string, const vkr::Type*> types;

  for (const vkr::Types& stypes : start.types)
    for (const vkr::Type& type : stypes.type) {
      if (type.alias) continue;
      std::string name = type.name_attribute.has_value()
                             ? type.name_attribute.value()
                             : type.name_subelement.value();
      dvc::insert_or_die(types, name, &type);
    }

  for (const vkr::Types& stypes : start.types)
    for (const vkr::Type& type : stypes.type) {
      if (!type.alias) continue;
      std::string name = type.name_attribute.has_value()
                             ? type.name_attribute.value()
                             : type.name_subelement.value();
      dvc::insert_or_die(types, name, types.at(type.alias.value()));
    }

  for (const vkr::Enums& enums : start.enums) {
    DVC_ASSERT(enums.name);
    std::string name = enums.name.value();
    if (name == "API Constants") continue;
    if (!types.count(name)) {
      DVC_LOG("no type enum: ", name);
      continue;
    }
    DVC_ASSERT(types.count(name));
    DVC_ASSERT(enums.type, enums.name.value());
    const vkr::Type& type = (*types.at(name));
    DVC_ASSERT_EQ(type.category.value(), "enum");
    auto enumeration = new vks::Enumeration;
    enumeration->name = name;
    DVC_ASSERT_NE(name, "VkPeerMemoryFeatureFlagBitsKHR");
    dvc::insert_or_die(registry.enumerations, name, enumeration);
    for (const vkr::Enum& enum_ : enums.enum_) {
      registry.enumerations.at(name)->enumerators.push_back(
          registry.constants.at(enum_.name));
    }
  }

  for (const vkr::Types& stypes : start.types)
    for (const vkr::Type& type : stypes.type) {
      std::string name = type.name_attribute.has_value()
                             ? type.name_attribute.value()
                             : type.name_subelement.value();
      if (type.category != "enum") continue;
      if (!type.alias) continue;
      dvc::insert_or_die(registry.enumerations, name,
                         registry.enumerations.at(type.alias.value()));
    }

  foreach_extension(
      registry, start, [&](auto require, auto extnumber, auto platform) {
        for (const auto& type : require.type) {
          if (registry.enumerations.count(type.name)) {
            registry.enumerations.at(type.name)->platform = platform;
            for (auto& constant :
                 registry.enumerations.at(type.name)->enumerators)
              registry.constants.at(constant->name)->platform = platform;
          }
        }
      });
}

void parse_bitmasks(vks::Registry& registry, const vkr::start& start) {
  for (const vkr::Types& stypes : start.types)
    for (const vkr::Type& type : stypes.type) {
      std::string name = type.name_attribute.has_value()
                             ? type.name_attribute.value()
                             : type.name_subelement.value();
      if (type.category != "bitmask") continue;
      if (type.alias) continue;
      auto bitmask = new vks::Bitmask;
      bitmask->name = name;
      bitmask->requires_ =
          (type.requires_ ? registry.enumerations.at(type.requires_.value())
                          : nullptr);

      dvc::insert_or_die(registry.bitmasks, name, bitmask);
    }

  for (const vkr::Types& stypes : start.types)
    for (const vkr::Type& type : stypes.type) {
      std::string name = type.name_attribute.has_value()
                             ? type.name_attribute.value()
                             : type.name_subelement.value();
      if (type.category != "bitmask") continue;
      if (!type.alias) continue;
      DVC_ASSERT(registry.bitmasks.count(type.alias.value()));
      dvc::insert_or_die(registry.bitmasks, name,
                         registry.bitmasks.at(type.alias.value()));
    }

  foreach_extension(registry, start,
                    [&](auto require, auto extnumber, auto platform) {
                      for (const auto& type : require.type) {
                        if (registry.bitmasks.count(type.name)) {
                          registry.bitmasks.at(type.name)->platform = platform;
                        }
                      }
                    });
}

void parse_handles(vks::Registry& registry, const vkr::start& start) {
  auto foreach_handle = [&](auto process_handle) {
    for (const auto& types : start.types)
      for (const vkr::Type& type : types.type) {
        if (type.category != "handle") continue;
        std::string name = type.name_attribute.has_value()
                               ? type.name_attribute.value()
                               : type.name_subelement.value();
        process_handle(type, name);
      }
  };

  foreach_handle([&](const vkr::Type& type, const std::string& name) {
    if (type.alias) return;
    std::string handle_type = type.type.at(0);
    DVC_ASSERT(handle_type == "VK_DEFINE_HANDLE" ||
               handle_type == "VK_DEFINE_NON_DISPATCHABLE_HANDLE");
    auto handle = new vks::Handle;
    handle->name = name;
    handle->dispatchable = (handle_type == "VK_DEFINE_HANDLE");
    dvc::insert_or_die(registry.handles, name, handle);
  });

  foreach_handle([&](const vkr::Type& type, const std::string& name) {
    if (!type.alias) return;
    dvc::insert_or_die(registry.handles, name,
                       registry.handles.at(type.alias.value()));
  });

  foreach_handle([&](const vkr::Type& type, const std::string& name) {
    if (type.alias) return;
    if (!type.parent) return;
    vks::Handle* handle = registry.handles.at(name);
    for (const std::string& parent : dvc::split(",", type.parent.value())) {
      handle->parents.insert(registry.handles.at(parent));
    }
  });
}

void parse_inner_text(std::ostream& o, relaxng::Element element,
                      const std::set<std::string>& skipelements) {
  for (auto p = element->FirstChild(); p != nullptr; p = p->NextSibling()) {
    if (auto text = p->ToText()) {
      std::string_view sv = text->Value();
      o.write(sv.data(), sv.size());
      o.write(" ", 1);
    } else if (auto subelement = p->ToElement()) {
      if (!skipelements.count(subelement->Name()))
        parse_inner_text(o, subelement, skipelements);
    }
  }
}

std::string parse_inner_text(relaxng::Element element) {
  std::set<std::string> skipelements = {"comment"};
  std::ostringstream o;
  parse_inner_text(o, element, skipelements);
  return o.str();
}

struct TypeBackpatches {
  struct StructMemberBackpatch {
    size_t member_idx;
    mnc::Type* type;
  };
  std::multimap<vks::Struct*, StructMemberBackpatch> struct_member_backpatches;
  void add_struct_member_backpatch(vks::Struct* struct_, size_t member_idx,
                                   mnc::Type* type) {
    struct_member_backpatches.insert(
        std::make_pair(struct_, StructMemberBackpatch{member_idx, type}));
  }

  std::map<vks::FunctionPrototype*, mnc::FunctionPrototype>
      function_prototype_backpatches;
  void add_function_prototype_backpatch(
      vks::FunctionPrototype* function_prototype,
      mnc::FunctionPrototype mnc_function_prototype) {
    dvc::insert_or_die(function_prototype_backpatches, function_prototype,
                       mnc_function_prototype);
  }

  std::map<vks::Command*, mnc::Type*> command_return_backpatches;
  void add_command_return_backpatch(vks::Command* command,
                                    mnc::Type* return_type) {
    dvc::insert_or_die(command_return_backpatches, command, return_type);
  }

  struct ParamBackpatch {
    size_t param_idx;
    mnc::Type* type;
  };
  std::multimap<vks::Command*, ParamBackpatch> command_param_backpatches;
  void add_command_param_backpatch(vks::Command* command, size_t param_idx,
                                   mnc::Type* type) {
    command_param_backpatches.insert(
        std::make_pair(command, ParamBackpatch{param_idx, type}));
  }
};

void parse_structs(vks::Registry& registry, TypeBackpatches& backpatches,
                   const vkr::start& start) {
  auto foreach_struct = [&](auto process_struct) {
    for (const auto& types : start.types)
      for (const vkr::Type& type : types.type) {
        if (type.category != "struct" && type.category != "union") continue;
        std::string name = type.name_attribute.has_value()
                               ? type.name_attribute.value()
                               : type.name_subelement.value();
        process_struct(type, name);
      }
  };

  foreach_struct([&](const vkr::Type& type, const std::string& name) {
    if (type.alias) return;
    bool is_union = (type.category == "union");

    auto struct_ = new vks::Struct;

    struct_->name = name;
    struct_->is_union = is_union;
    if (type.returnedonly) {
      DVC_ASSERT(type.returnedonly == "true");
      struct_->returnedonly = true;
    } else
      struct_->returnedonly = false;
    dvc::insert_or_die(registry.structs, name, struct_);
  });

  foreach_struct([&](const vkr::Type& type, const std::string& name) {
    if (!type.alias) return;
    dvc::insert_or_die(registry.structs, name,
                       registry.structs.at(type.alias.value()));
  });

  foreach_struct([&](const vkr::Type& type, const std::string& name) {
    if (type.structextends)
      for (const std::string& structextends :
           dvc::split(",", type.structextends.value()))
        registry.structs.at(name)->structextends.push_back(
            registry.structs.at(structextends));
  });

  foreach_struct([&](const vkr::Type& type, const std::string& name) {
    if (type.alias) return;
    vks::Struct* struct_ = registry.structs.at(name);
    for (size_t member_idx = 0; member_idx < type.member.size(); member_idx++) {
      const vkr::Type_member& member_in = type.member.at(member_idx);
      vks::Member member_out;
      DVC_ASSERT(member_in.name.has_value());
      member_out.name = member_in.name.value();
      if (member_in.values) {
        DVC_ASSERT(member_idx == 0);
        DVC_ASSERT(member_in.name == "sType");
        DVC_ASSERT(member_in.type == "VkStructureType");
        DVC_ASSERT(type.member.at(1).name == "pNext");
        if (registry.constants.count(member_in.values.value())) {
          struct_->structured_type =
              registry.constants.at(member_in.values.value());
          member_idx++;
          continue;
        }
      }

      if (member_in.altlen) {
        DVC_ASSERT(member_in.len);
        for (const std::string& s : dvc::split(",", member_in.altlen.value()))
          member_out.len.push_back(s);
      } else if (member_in.len) {
        for (const std::string& s : dvc::split(",", member_in.len.value()))
          member_out.len.push_back(s);
      }

      if (member_in.optional) {
        for (const std::string& s :
             dvc::split(",", member_in.optional.value())) {
          DVC_ASSERT(s == "true" || s == "false");
          member_out.optional.push_back(s == "true" ? true : false);
        }
      }

      mnc::Declaration decl =
          mnc::parse_declaration(parse_inner_text(member_in._element_));
      DVC_ASSERT_EQ(member_in.name.value(), decl.name);
      mnc::Type* member_type = decl.type;
      backpatches.add_struct_member_backpatch(struct_, struct_->members.size(),
                                              member_type);

      struct_->members.push_back(member_out);
    }
  });

  foreach_extension(registry, start,
                    [&](auto require, auto extnumber, auto platform) {
                      for (const auto& type : require.type) {
                        if (registry.structs.count(type.name)) {
                          registry.structs.at(type.name)->platform = platform;
                        }
                      }
                    });
}

void parse_funcpointers(vks::Registry& registry, TypeBackpatches& backpatches,
                        const vkr::start& start) {
  auto foreach_funcpointer = [&](auto process_funcpointer) {
    for (const auto& types : start.types)
      for (const vkr::Type& type : types.type) {
        if (type.category != "funcpointer") continue;
        std::string name = type.name_attribute.has_value()
                               ? type.name_attribute.value()
                               : type.name_subelement.value();
        process_funcpointer(type, name);
      }
  };

  foreach_funcpointer([&](const vkr::Type& type, const std::string& name) {
    DVC_ASSERT(!type.alias);
    auto function_prototype_out = new vks::FunctionPrototype;
    function_prototype_out->name = name;
    std::string decl = parse_inner_text(type._element_);
    mnc::FunctionPrototype function_prototype_in =
        mnc::parse_function_prototype(decl);
    DVC_ASSERT_EQ(name, function_prototype_in.name);

    backpatches.add_function_prototype_backpatch(function_prototype_out,
                                                 function_prototype_in);
    dvc::insert_or_die(registry.function_prototypes, name,
                       function_prototype_out);
  });
}

void parse_commands(vks::Registry& registry, TypeBackpatches& backpatches,
                    const vkr::start& start) {
  auto foreach_command = [&](auto process_command) {
    for (const vkr::Commands& commands : start.commands)
      for (const vkr::Command& command : commands.command) {
        DVC_ASSERT(!command.alias_subelement);
        process_command(command);
      }
  };
  foreach_command([&](const vkr::Command& command_in) {
    if (command_in.alias_attribute) return;
    std::string name = command_in.proto.value().name;
    std::vector<std::string> errorcodes, successcodes;
    if (command_in.errorcodes) {
      errorcodes = dvc::split(",", command_in.errorcodes.value());
    }
    if (command_in.successcodes) {
      successcodes = dvc::split(",", command_in.successcodes.value());
    }
    auto command_out = new vks::Command;
    command_out->name = name;

    for (const std::string& errorcode : errorcodes) {
      command_out->errorcodes.push_back(registry.constants.at(errorcode));
    }

    for (const std::string& successcode : successcodes) {
      command_out->successcodes.push_back(registry.constants.at(successcode));
    }

    DVC_ASSERT(command_in.proto.has_value());
    mnc::Declaration decl = mnc::parse_declaration(
        parse_inner_text(command_in.proto.value()._element_));
    DVC_ASSERT_EQ(decl.name, name);
    backpatches.add_command_return_backpatch(command_out, decl.type);
    for (const vkr::Command_param& param_in : command_in.param) {
      mnc::Declaration decl =
          mnc::parse_declaration(parse_inner_text(param_in._element_));
      DVC_ASSERT_EQ(decl.name, param_in.name.value());
      vks::Param param_out;
      param_out.name = decl.name;

      if (param_in.optional)
        for (const std::string& opt :
             dvc::split(",", param_in.optional.value())) {
          DVC_ASSERT(opt == "true" || opt == "false");
          param_out.optional.push_back(opt == "true");
        }

      if (param_in.len) param_out.len = param_in.len;

      backpatches.add_command_param_backpatch(
          command_out, command_out->params.size(), decl.type);
      command_out->params.push_back(param_out);
    }
    dvc::insert_or_die(registry.commands, name, command_out);
  });
  foreach_command([&](const vkr::Command& command) {
    if (!command.alias_attribute) return;
    std::string name = command.name.value();
    std::string alias = command.alias_attribute.value();
    dvc::insert_or_die(registry.commands, name, registry.commands.at(alias));
  });
  foreach_extension(registry, start,
                    [&](auto require, auto extnumber, auto platform) {
                      for (const auto& command : require.command)
                        registry.commands.at(command.name)->platform = platform;
                    });
}

vks::Entity* lookup_entity(vks::Registry& registry, const std::string& name) {
  DVC_ASSERT(registry.entities.count(name), "No such entity: ", name);
  return registry.entities.at(name);
}

vks::Expr* translate_expr(vks::Registry& registry, mnc::Expr* expr) {
  if (auto reference = dynamic_cast<mnc::Reference*>(expr)) {
    auto result = new vks::Reference;
    result->entity = lookup_entity(registry, reference->name);
    return result;
  } else if (auto number = dynamic_cast<mnc::Number*>(expr)) {
    auto result = new vks::Number;
    result->number = number->number;
    return result;
  } else {
    DVC_FATAL("Unknown expr: ", typeid(expr).name());
  }
}

vks::Type* translate_type(vks::Registry& registry, mnc::Type* type) {
  if (auto name = dynamic_cast<mnc::Name*>(type)) {
    auto result = new vks::Name;
    result->entity = lookup_entity(registry, name->name);
    return result;
  } else if (auto const_ = dynamic_cast<mnc::Const*>(type)) {
    auto result = new vks::Const;
    result->T = translate_type(registry, const_->T);
    return result;
  } else if (auto pointer = dynamic_cast<mnc::Pointer*>(type)) {
    auto result = new vks::Pointer;
    result->T = translate_type(registry, pointer->T);
    return result;
  } else if (auto array = dynamic_cast<mnc::Array*>(type)) {
    auto result = new vks::Array;
    result->T = translate_type(registry, array->T);
    result->N = translate_expr(registry, array->N);
    return result;
  }
  DVC_ERROR("Unknown mnc type: ", typeid(type).name());
  return nullptr;  // ???
}

void apply_backpatches(vks::Registry& registry, TypeBackpatches& backpatches) {
  for (const auto& [name, struct_] : registry.structs) {
    (void)name;
    auto backpatch = backpatches.struct_member_backpatches.equal_range(struct_);
    for (auto it = backpatch.first; it != backpatch.second; ++it) {
      vks::Member& member = struct_->members.at(it->second.member_idx);
      member.type = translate_type(registry, it->second.type);
    }
  }

  for (const auto& [name, function_prototype] : registry.function_prototypes) {
    (void)name;
    mnc::FunctionPrototype mnc_function_prototype =
        backpatches.function_prototype_backpatches.at(function_prototype);
    function_prototype->return_type =
        translate_type(registry, mnc_function_prototype.return_type);
    for (const auto& param_in : mnc_function_prototype.params) {
      vks::FunctionPrototypeParam param_out;
      param_out.name = param_in.name;
      param_out.type = translate_type(registry, param_in.type);
      function_prototype->params.push_back(param_out);
    }
  }

  for (const auto& [name, command] : registry.commands) {
    (void)name;
    mnc::Type* return_backpatch =
        backpatches.command_return_backpatches.at(command);
    command->return_type = translate_type(registry, return_backpatch);
    auto params_backpatch =
        backpatches.command_param_backpatches.equal_range(command);
    for (auto it = params_backpatch.first; it != params_backpatch.second;
         ++it) {
      vks::Param& param = command->params.at(it->second.param_idx);
      param.type = translate_type(registry, it->second.type);
    }
  }
}

void populate_entities(vks::Registry& registry) {
  auto pe = [&](const auto& m) {
    for (const auto& [k, v] : m) dvc::insert_or_die(registry.entities, k, v);
  };

  pe(registry.constants);
  pe(registry.enumerations);
  pe(registry.bitmasks);
  pe(registry.handles);
  pe(registry.structs);
  pe(registry.function_prototypes);
  pe(registry.commands);
  pe(registry.externals);
}

void remove_disabled(vks::Registry& registry, const vkr::start& start) {
  for (const vkr::Extensions& extensions : start.extensions) {
    for (const vkr::Extension& extension : extensions.extension) {
      std::string supported = extension.supported.value();
      DVC_ASSERT(supported == "disabled" || supported == "vulkan", supported);
      if (supported == "vulkan") continue;

      for (const vkr::Extension_require& require : extension.require) {
        for (auto command : require.command) {
          registry.commands.erase(command.name);
          registry.entities.erase(command.name);
        }

        for (auto struct_ : require.type) {
          if (registry.structs.count(struct_.name) == 0) continue;
          registry.structs.erase(struct_.name);
          registry.entities.erase(struct_.name);
        }
      }
    }
  }
}

void apply_constant_extends(
    vks::Registry& registry,
    std::multimap<std::string, vks::Constant*>& extends) {
  for (const auto& [extend, constant] : extends) {
    DVC_ASSERT(registry.enumerations.count(extend),
               "Could not find extension enumeration: ", extend, " for ",
               constant->name);
    registry.enumerations.at(extend)->enumerators.push_back(constant);
  }
}

void build_dispatch_tables(vks::Registry& registry) {
  auto relate_dispatch_table = [&](vks::DispatchTableKind kind,
                                   vks::Command* command) {
    vks::DispatchTable* dispatch_table = registry.dispatch_table(kind);
    dispatch_table->commands.push_back(command);
    DVC_ASSERT(command->dispatch_table == nullptr);
    command->dispatch_table = dispatch_table;
  };
  for (vks::DispatchTableKind kind :
       {vks::DispatchTableKind::GLOBAL, vks::DispatchTableKind::INSTANCE,
        vks::DispatchTableKind::DEVICE}) {
    auto table = new vks::DispatchTable;
    table->kind = kind;
    registry.dispatch_table(kind) = table;
  }
  for (const auto& [name, command] : registry.commands) {
    if (name != command->name) continue;
    static std::set<std::string> global_command_names = {
        "vkEnumerateInstanceVersion", "vkEnumerateInstanceExtensionProperties",
        "vkEnumerateInstanceLayerProperties", "vkCreateInstance",
        "vkGetInstanceProcAddr"};
    if (global_command_names.count(name)) {
      relate_dispatch_table(vks::DispatchTableKind::GLOBAL, command);
      continue;
    }
    auto first_param_type =
        dynamic_cast<const vks::Name*>(command->params.at(0).type);
    DVC_ASSERT(first_param_type);
    auto dispatch_handle =
        dynamic_cast<const vks::Handle*>(first_param_type->entity);
    DVC_ASSERT(dispatch_handle);
    if (dispatch_handle->name == "VkInstance" ||
        dispatch_handle->name == "VkPhysicalDevice") {
      relate_dispatch_table(vks::DispatchTableKind::INSTANCE, command);
    } else {
      relate_dispatch_table(vks::DispatchTableKind::DEVICE, command);
    }
  }
}

}  // namespace

vks::Registry parse_registry(const vkr::start& start) {
  vks::Registry registry;

  parse_platforms(registry, start);
  parse_externals(registry, start);
  std::multimap<std::string, vks::Constant*> extends;
  parse_constants(registry, extends, start);
  parse_enumerations(registry, start);
  parse_bitmasks(registry, start);
  parse_handles(registry, start);
  TypeBackpatches backpatches;
  parse_structs(registry, backpatches, start);
  parse_funcpointers(registry, backpatches, start);
  parse_commands(registry, backpatches, start);
  populate_entities(registry);
  apply_backpatches(registry, backpatches);
  apply_constant_extends(registry, extends);
  remove_disabled(registry, start);
  build_dispatch_tables(registry);

  return registry;
}
