#include "sps/spsbuilder.h"

#include <glog/logging.h>
#include <cctype>
#include <clocale>
#include <set>

#include "dvc/container.h"
#include "dvc/string.h"
#include "sps/spsaccessors.h"

namespace {

enum charkind { underscore, uppercase, lowercase, digit };

charkind to_charkind(char c) {
  if (c == '_') return underscore;
  if (std::isdigit(c)) return digit;
  if (std::islower(c)) return lowercase;
  if (std::isupper(c)) return uppercase;
  LOG(FATAL) << "unexpected char: " << c;
}

std::vector<std::string_view> split_identifier_view(
    std::string_view identifier) {
  std::vector<size_t> poses;
  for (size_t i = 0; i < identifier.size() - 1; i++) {
    charkind a = to_charkind(identifier[i]);
    charkind b = to_charkind(identifier[i + 1]);
    if (a == lowercase && b == uppercase)
      poses.push_back(i + 1);
    else if (a == lowercase && b == digit) {
      poses.push_back(i + 1);
    } else if (a == uppercase && b == uppercase && i + 2 < identifier.size() &&
               to_charkind(identifier[i + 2]) == lowercase)
      poses.push_back(i + 1);
    else if (a == underscore) {
      poses.push_back(i);
      poses.push_back(i + 1);
    }
  }
  std::vector<std::string_view> splits;
  size_t n = poses.size();
  for (size_t i = 0; i < n + 1; i++) {
    size_t f = (i == 0 ? 0 : poses[i - 1]);
    size_t l = (i == n ? identifier.size() : poses[i]);
    if (l - f == 1 && identifier[f] == '_') continue;
    splits.push_back(identifier.substr(f, l - f));
  }
  return splits;
}

std::string to_underscore_style(const std::vector<std::string_view>& s) {
  std::string o;
  size_t t = s.size() - 1;
  for (auto x : s) t += x.size();
  o.resize(t);
  size_t pos = 0;
  bool first = true;
  for (auto x : s) {
    if (!first) {
      o[pos] = '_';
      pos++;
    } else {
      first = false;
    }

    for (size_t i = 0; i < x.size(); i++) o[pos + i] = std::tolower(x[i]);
    pos += x.size();
  }

  return o;
}

std::string translate_enumeration_name(std::string_view name) {
  CHECK(name.substr(0, 2) == "Vk") << name;
  return to_underscore_style(split_identifier_view(name.substr(2)));
}

std::string translate_bitmask_name(std::string_view name) {
  CHECK(name.substr(0, 2) == "Vk") << name;
  return to_underscore_style(split_identifier_view(name.substr(2)));
}

std::string translate_handle_name(std::string_view name) {
  CHECK(name.substr(0, 2) == "Vk") << name;
  return to_underscore_style(split_identifier_view(name.substr(2)));
}

std::string translate_struct_name(std::string_view name) {
  CHECK(name.substr(0, 2) == "Vk") << name;
  return to_underscore_style(split_identifier_view(name.substr(2)));
}

std::string translate_enumerator_name(const std::string& name) {
  CHECK(name.substr(0, 3) == "VK_") << name;
  return to_underscore_style(split_identifier_view(name.substr(3)));
}

std::string translate_command_name(const std::string& name) {
  CHECK(name.substr(0, 2) == "vk") << name;
  return to_underscore_style(split_identifier_view(name.substr(2)));
}

std::string translate_member_name(const std::string& name) {
  return to_underscore_style(split_identifier_view(name));
}

std::string common_prefix(const std::vector<std::string>& names) {
  CHECK_GT(names.size(), 1);
  for (size_t i = 0; i <= names[0].size(); i++) {
    char c = names[0][i];
    for (size_t j = 1; j < names.size(); j++)
      if (c != names[j][i]) return names[0].substr(0, i);
  }
  LOG(FATAL) << "prefix not proper: " << names[0];
}

// std::string translate_constant_name(const std::string& name) {
//  CHECK(name.substr(0, 3) == "VK_") << name;
//  return name.substr(3);
//}

std::string final_enum_fix(const std::string id) {
  static std::set<std::string> keywords = {"and", "xor", "or", "inline",
                                           "protected"};
  if (keywords.count(id)) return id + "_";
  if (std::isdigit(id[0])) return "n" + id;
  return id;
}

const vks::Type* translate_member_type(const vks::Registry& vreg,
                                       const sps::Registry& sreg,
                                       const vks::Type* vtype) {
  if (auto name = dynamic_cast<const vks::Name*>(vtype)) {
    const sps::Entity* entity = nullptr;
    if (auto handle = dynamic_cast<const vks::Handle*>(name->entity)) {
      entity = sreg.handle_map.at(handle);
    } else if (auto bitmask = dynamic_cast<vks::Bitmask*>(name->entity)) {
      entity = sreg.bitmask_map.at(bitmask);
    } else if (auto enumeration =
                   dynamic_cast<vks::Enumeration*>(name->entity)) {
      if (sreg.flag_bits_map.count(enumeration))
        entity = sreg.flag_bits_map.at(enumeration);
      else
        entity = sreg.enumeration_map.at(enumeration);
    } else if (auto struct_ = dynamic_cast<vks::Struct*>(name->entity)) {
      entity = sreg.struct_map.at(struct_);
    } else if (auto external = dynamic_cast<vks::External*>(name->entity)) {
      if (external->name == "VkDeviceSize") {
        auto uint64_entity = new vks::External;
        uint64_entity->name = "uint64_t";
        auto uint64_name = new vks::Name;
        uint64_name->entity = uint64_entity;
        return uint64_name;
      } else if (external->name == "VkBool32") {
        auto bool32_entity = new vks::External;
        bool32_entity->name = "spk::bool32_t";
        auto bool32_name = new vks::Name;
        bool32_name->entity = bool32_entity;
        return bool32_name;
      } else {
        return name;
      }
    } else if (dynamic_cast<vks::FunctionPrototype*>(name->entity)) {
      return name;
    } else {
      LOG(FATAL) << "Unknown entity sublass: " << typeid(*name->entity).name();
    }

    if (entity != nullptr) {
      auto T = new sps::Name;
      T->entity = entity;
      return T;
    } else {
      return name;
    }
  } else if (auto pointer = dynamic_cast<const vks::Pointer*>(vtype)) {
    auto T = new vks::Pointer;
    T->T = translate_member_type(vreg, sreg, pointer->T);
    return T;
  } else if (auto const_ = dynamic_cast<const vks::Const*>(vtype)) {
    auto T = new vks::Const;
    T->T = translate_member_type(vreg, sreg, const_->T);
    return T;
  } else if (auto array = dynamic_cast<const vks::Array*>(vtype)) {
    auto T = new vks::Array;
    T->N = array->N;
    T->T = translate_member_type(vreg, sreg, array->T);
    return T;
  } else {
    LOG(FATAL) << "unknown vks::Type* subclass: " << typeid(*vtype).name();
  }
}

const vks::Type* translate_param_type(const vks::Registry& vreg,
                                      const sps::Registry& sreg,
                                      const vks::Type* vtype) {
  return translate_member_type(vreg, sreg, vtype);
}

auto sort_on_name = [](auto& container) {
  std::sort(container.begin(), container.end(),
            [](auto a, auto b) { return a->name < b->name; });
};

void build_enum(sps::Registry& sreg, const vks::Registry& vreg) {
  std::unordered_set<const vks::Constant*> constants_done;

  auto convert_enumeration = [&](std::string name,
                                 const vks::Enumeration* venumeration) {
    auto senumeration = new sps::Enumeration;
    senumeration->name = translate_enumeration_name(name);
    senumeration->enumeration = venumeration;
    for (const auto& venumerator : venumeration->enumerators) {
      sps::Enumerator senumerator;
      senumerator.name = translate_enumerator_name(venumerator->name);
      senumerator.constant = venumerator;
      constants_done.insert(venumerator);

      senumeration->enumerators.push_back(senumerator);
    }
    std::vector<std::string> unstripped_names;
    unstripped_names.push_back(senumeration->name + "_");
    for (const auto& senumerator : senumeration->enumerators)
      unstripped_names.push_back(senumerator.name);
    size_t strip = common_prefix(unstripped_names).size();
    while (strip != 0) {
      if (unstripped_names.at(0).at(strip - 1) == '_') break;
      strip--;
    }
    for (auto& senumerator : senumeration->enumerators)
      senumerator.name = final_enum_fix(senumerator.name.substr(strip));
    for (const auto& [aname, alias] : vreg.enumerations) {
      if (venumeration == alias && aname != alias->name) {
        senumeration->aliases.push_back(translate_enumeration_name(aname));
      }
    }
    dvc::sort(senumeration->aliases);
    return senumeration;
  };

  for (const auto& [name, vbitmask] : vreg.bitmasks) {
    if (name != vbitmask->name) continue;
    sps::Bitmask* bitmask = new sps::Bitmask;
    bitmask->name = translate_bitmask_name(name);
    bitmask->bitmask = vbitmask;
    if (vbitmask->requires) {
      dvc::insert_or_die(sreg.flag_bits_map, vbitmask->requires, bitmask);
      sps::Enumeration* enumeration =
          convert_enumeration(name, vbitmask->requires);
      for (sps::Enumerator enumerator : enumeration->enumerators) {
        size_t bitpos = enumerator.name.rfind("_bit");
        if (bitpos != std::string::npos)
          enumerator.name = final_enum_fix(enumerator.name.substr(0, bitpos) +
                                           enumerator.name.substr(bitpos + 4));
        bitmask->enumerators.push_back(enumerator);
      }
    }
    for (const auto& [aname, alias] : vreg.bitmasks) {
      if (vbitmask == alias && aname != alias->name) {
        bitmask->aliases.push_back(translate_bitmask_name(aname));
      }
    }
    sreg.bitmasks.push_back(bitmask);
  }

  sort_on_name(sreg.bitmasks);

  for (auto bitmask : sreg.bitmasks) {
    dvc::insert_or_die(sreg.bitmask_map, bitmask->bitmask, bitmask);
  }

  for (const auto& [name, venumeration] : vreg.enumerations) {
    if (sreg.flag_bits_map.count(venumeration)) continue;
    if (name != venumeration->name) continue;
    sreg.enumerations.push_back(convert_enumeration(name, venumeration));
  }

  sort_on_name(sreg.enumerations);

  for (auto enumeration : sreg.enumerations) {
    dvc::insert_or_die(sreg.enumeration_map, enumeration->enumeration,
                       enumeration);
  }

  for (const auto& [name, vconstant] : vreg.constants) {
    if (constants_done.count(vconstant)) continue;
    if (name == "VK_TRUE" || name == "VK_FALSE") continue;
    sps::Constant* sconstant = new sps::Constant;
    sconstant->name = translate_enumerator_name(name);
    sconstant->constant = vconstant;
    sreg.constants.push_back(sconstant);
  }
  sort_on_name(sreg.constants);
}

void build_handle(sps::Registry& sreg, const vks::Registry& vreg) {
  for (const auto& [name, vhandle] : vreg.handles) {
    if (name != vhandle->name) continue;
    auto shandle = new sps::Handle;
    shandle->name = translate_handle_name(name) + "_ref";
    shandle->fullname = translate_handle_name(name);
    shandle->handle = vhandle;
    for (const auto& [aname, alias] : vreg.handles) {
      if (vhandle == alias && aname != alias->name) {
        shandle->aliases.push_back(translate_handle_name(aname));
      }
    }
    sreg.handles.push_back(shandle);
  }
  sort_on_name(sreg.handles);

  for (auto handle : sreg.handles) {
    dvc::insert_or_die(sreg.handle_map, handle->handle, handle);
  }

  for (auto handle : sreg.handles) {
    for (auto parent : handle->handle->parents)
      handle->parents.insert(sreg.handle_map.at(parent));
  }
}

void build_struct(sps::Registry& sreg, const vks::Registry& vreg) {
  for (const auto& [name, vstruct] : vreg.structs) {
    if (name != vstruct->name) continue;
    auto sstruct = new sps::Struct;
    sstruct->name = translate_struct_name(name);
    sstruct->struct_ = vstruct;
    for (size_t member_idx = 0; member_idx < vstruct->members.size();
         member_idx++) {
      const vks::Member& vmember = vstruct->members.at(member_idx);
      sps::Member smember;
      smember.name = translate_member_name(vmember.name) + "_";
      smember.len = vmember.len;
      if (smember.len.size() > 0 && smember.len.back() == "null-terminated") {
        smember.null_terminated = true;
        smember.len.pop_back();
      }
      smember.optional_ = vmember.optional;
      smember.vtype = vmember.type;
      sstruct->members.push_back(smember);
    }

    for (const auto& [aname, alias] : vreg.structs) {
      if (vstruct == alias && aname != alias->name) {
        sstruct->aliases.push_back(translate_struct_name(aname));
      }
    }
    sreg.structs.push_back(sstruct);
  }

  for (auto struct_ : sreg.structs) {
    dvc::insert_or_die(sreg.struct_map, struct_->struct_, struct_);
  }

  std::unordered_set<sps::Struct*> todo_structs(sreg.structs.begin(),
                                                sreg.structs.end());
  sreg.structs.clear();

  auto struct_deps = [&](sps::Struct* struct_) {
    std::vector<sps::Struct*> deps;
    for (const vks::Member& member : struct_->struct_->members) {
      const vks::Entity* entity;
      bool complete;
      member.type->get_entity_dep(entity, complete);
      if (complete)
        if (auto vstruct = dynamic_cast<const vks::Struct*>(entity))
          deps.push_back(sreg.struct_map.at(vstruct));
    }
    return deps;
  };
  while (!todo_structs.empty()) {
    std::vector<sps::Struct*> structs_done;
    for (sps::Struct* struct_ : todo_structs) {
      bool depsok = true;
      for (sps::Struct* dep : struct_deps(struct_)) {
        if (todo_structs.count(dep)) {
          depsok = false;
          break;
        }
      }
      if (depsok) {
        structs_done.push_back(struct_);
      }
    }
    if (structs_done.empty()) LOG(FATAL) << "Circular dependency in structs";
    sort_on_name(structs_done);
    for (sps::Struct* struct_ : structs_done) {
      CHECK(todo_structs.erase(struct_));
      sreg.structs.push_back(struct_);
    }
  }

  for (sps::Struct* struct_ : sreg.structs) {
    for (sps::Member& member : struct_->members) {
      member.stype = translate_member_type(vreg, sreg, member.vtype);
    }
  }
}

void build_command(sps::Registry& sreg, const vks::Registry& vreg) {
  for (const sps::Enumeration* enumeration : sreg.enumerations) {
    if (enumeration->name != "result") continue;
    CHECK_EQ(enumeration->enumerators.size(),
             enumeration->enumeration->enumerators.size());
    for (size_t i = 0; i < enumeration->enumerators.size(); i++) {
      sreg.codemap[enumeration->enumeration->enumerators.at(i)] =
          &(enumeration->enumerators.at(i));
    }
  }

  for (const auto& [name, vcommand] : vreg.commands) {
    auto scommand = new sps::Command;
    scommand->command = vcommand;
    scommand->name = translate_command_name(name);

    scommand->vreturn_type = vcommand->return_type;
    scommand->sreturn_type =
        translate_param_type(vreg, sreg, vcommand->return_type);

    for (const vks::Param& vparam : vcommand->params) {
      sps::Param sparam;
      sparam.param = &vparam;
      sparam.name = vparam.name;
      sparam.vtype = vparam.type;
      sparam.stype = translate_param_type(vreg, sreg, vparam.type);
      scommand->params.push_back(sparam);
    }

    for (const vks::Constant* successcode : vcommand->successcodes)
      scommand->successcodes.push_back(sreg.codemap.at(successcode));
    for (const vks::Constant* errorcode : vcommand->errorcodes)
      scommand->errorcodes.push_back(sreg.codemap.at(errorcode));

    sreg.commands.push_back(scommand);
    sreg.command_map[vcommand].push_back(scommand);
  }

  for (vks::DispatchTableKind dispatch_table_kind :
       {vks::DispatchTableKind::GLOBAL, vks::DispatchTableKind::INSTANCE,
        vks::DispatchTableKind::DEVICE}) {
    const vks::DispatchTable* vdispatch_table =
        vreg.dispatch_table(dispatch_table_kind);
    sps::DispatchTable*& sdispatch_table =
        sreg.dispatch_table(dispatch_table_kind);
    sdispatch_table = new sps::DispatchTable;

    sdispatch_table->dispatch_table = vdispatch_table;
    for (const vks::Command* vcommand : vdispatch_table->commands) {
      for (const sps::Command* scommand : sreg.command_map.at(vcommand)) {
        sdispatch_table->commands.push_back(scommand);
      }
    }
  }

  auto get_handle = [](const vks::Type* t) -> const sps::Handle* {
    auto n = dynamic_cast<const sps::Name*>(t);
    if (!n) return nullptr;
    return dynamic_cast<const sps::Handle*>(n->entity);
  };

  auto get_ptr_handle = [&](const vks::Type* t) -> const sps::Handle* {
    auto p = dynamic_cast<const vks::Pointer*>(t);
    if (!p) return nullptr;
    return get_handle(p->T);
  };

  for (const sps::Command* command : sreg.commands) {
    if (command->params.size() == 0) return;
    const sps::Handle* chandle = nullptr;
    sps::MemberFunctionKind kind;
    if (dvc::startswith(command->name, "create") ||
        dvc::startswith(command->name, "allocate")) {
      chandle = get_ptr_handle(command->params.back().stype);
      kind = sps::MemberFunctionKind::CONSTRUCTOR;
      CHECK(chandle);
    } else {
      const sps::Handle* first_handle = get_handle(command->params.at(0).stype);
      if (!first_handle) continue;
      const sps::Handle* second_handle = nullptr;
      if (command->params.size() > 1) {
        second_handle = get_handle(command->params.at(1).stype);
      }
      if (second_handle &&
          (!second_handle->handle->parents.count(first_handle->handle) ||
           !command->params.at(1).param->get_optional(0)))
        second_handle = nullptr;

      if (dvc::startswith(command->name, "destroy_")) {
        kind = second_handle ? sps::MemberFunctionKind::DOUBLE_DESTRUCTOR
                             : sps::MemberFunctionKind::SINGLE_DESTRUCTOR;

      } else {
        kind = second_handle ? sps::MemberFunctionKind::DOUBLE_HANDLE
                             : sps::MemberFunctionKind::SINGLE_HANDLE;
      }
      chandle = second_handle ? second_handle : first_handle;
    }

    sps::Handle* handle = sreg.handle_map.at(chandle->handle);
    CHECK(chandle == handle);
    sps::MemberFunction member_function;
    member_function.kind = kind;
    member_function.command = command;
    handle->member_functions.push_back(member_function);
  }
}

}  // namespace

sps::Registry build_spock_registry(const vks::Registry& vreg) {
  sps::Registry sreg;
  sreg.vreg = &vreg;

  build_enum(sreg, vreg);
  build_handle(sreg, vreg);
  build_struct(sreg, vreg);
  sps::add_accessors(sreg, vreg);
  build_command(sreg, vreg);

  return sreg;
}
