#include "sps/spsbuilder.h"

#include <glog/logging.h>
#include <cctype>
#include <clocale>
#include <map>
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
  std::string mname = to_underscore_style(split_identifier_view(name));
  if (mname == "major" || mname == "minor") mname = mname + "_";
  return mname;
}

std::string translate_member_function_name(const std::string& handle,
                                           const std::string& command) {
  std::string name = command;
  if (dvc::startswith(command, "get_")) name = name.substr(4);
  auto pos = name.find(handle + "_");
  if (pos != std::string::npos)
    name = name.substr(0, pos) + name.substr(pos + (handle + "_").size());
  pos = name.find("_" + handle);
  if (pos != std::string::npos)
    name = name.substr(0, pos) + name.substr(pos + ("_" + handle).size());
  if (handle == "command_buffer" && dvc::startswith(name, "cmd_"))
    name = name.substr(4);
  return name;
}

std::string common_prefix(const std::vector<std::string>& names) {
  if (names.size() < 2) return "";
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

std::set<std::string> manual_translation_commands = {
    "enumerate_instance_layer_properties",
    "enumerate_instance_extension_properties",
    "create_instance",
    "enumerate_instance_version",
    "get_instance_proc_addr",
    "get_device_proc_addr",
    "destroy_descriptor_update_template_khr",
    "destroy_sampler_ycbcr_conversion_khr",
    "debug_report_message_ext",
};

}  // namespace
namespace sps {
std::vector<sps::ManualTranslation> Registry::manual_translations = {
    {"physical_device", "create_device",

     R"(
    inline spk::device create_device(
        spk::device_create_info const& pCreateInfo);
)",
     R"(inline spk::device physical_device::create_device(
    spk::device_create_info const& create_info) {
  spk::device_ref device_ref;
  dispatch_table().create_device(handle_, &create_info, allocation_callbacks_,
                                 &device_ref);
  return device(device_ref, *this, allocation_callbacks_);
}
)"},
    {"physical_device", "get_physical_device_generated_commands_properties_nvx",
     R"(
inline std::pair<spk::device_generated_commands_features_nvx,spk::device_generated_commands_limits_nvx>
generated_commands_properties_nvx();
)",
     R"(
inline std::pair<spk::device_generated_commands_features_nvx,spk::device_generated_commands_limits_nvx>
physical_device::generated_commands_properties_nvx() {
  std::pair<spk::device_generated_commands_features_nvx,spk::device_generated_commands_limits_nvx> result_;
  dispatch_table().get_physical_device_generated_commands_properties_nvx(handle_, &result_.first, &result_.second);
  return result_;
};)"},
    {"device", "create_graphics_pipelines",
     R"(
inline std::vector<spk::pipeline>
create_graphics_pipelines(spk::pipeline_cache_ref pipeline_cache,
                          spk::array_view<const spk::graphics_pipeline_create_info> create_infos);
)",
     R"(
inline std::vector<spk::pipeline>
device::create_graphics_pipelines(spk::pipeline_cache_ref pipeline_cache,
                          spk::array_view<const spk::graphics_pipeline_create_info> create_infos) {
  std::vector<spk::pipeline_ref> pipeline_refs(create_infos.size());
  dispatch_table().create_graphics_pipelines(handle_,pipeline_cache, create_infos.size(), create_infos.data(), allocation_callbacks_, pipeline_refs.data());
  std::vector<spk::pipeline> pipelines;
  pipelines.reserve(create_infos.size());
  for (auto pipeline_ref : pipeline_refs)
    pipelines.emplace_back(pipeline_ref,*this,dispatch_table(),allocation_callbacks_);
  return pipelines;
}
)"},
    {
        "device",
        "allocate_command_buffers",
        R"(
inline std::vector<spk::command_buffer>
allocate_command_buffers(spk::command_buffer_allocate_info& allocate_info);
)",
        R"(
inline std::vector<spk::command_buffer>
device::allocate_command_buffers(spk::command_buffer_allocate_info& allocate_info) {
  std::vector<spk::command_buffer_ref> command_buffer_refs(allocate_info.command_buffer_count());
  dispatch_table().allocate_command_buffers(handle_, &allocate_info, command_buffer_refs.data());
  std::vector<spk::command_buffer> command_buffers;
  command_buffers.reserve(allocate_info.command_buffer_count());
  for (auto command_buffer_ref : command_buffer_refs)
    command_buffers.emplace_back(command_buffer_ref,*this,dispatch_table(),allocation_callbacks_);
  return command_buffers;
}
)",
    },
    {
        "device",
        "allocate_descriptor_sets",
        R"(
inline std::vector<spk::descriptor_set>
allocate_descriptor_sets(spk::descriptor_set_allocate_info& allocate_info);
)",
        R"(
inline std::vector<spk::descriptor_set>
device::allocate_descriptor_sets(spk::descriptor_set_allocate_info& allocate_info) {
  std::vector<spk::descriptor_set_ref> descriptor_set_refs(allocate_info.set_layouts().size());
  dispatch_table().allocate_descriptor_sets(handle_, &allocate_info, descriptor_set_refs.data());
  std::vector<spk::descriptor_set> descriptor_sets;
  descriptor_sets.reserve(allocate_info.set_layouts().size());
  for (auto descriptor_set_ref : descriptor_set_refs)
    descriptor_sets.emplace_back(descriptor_set_ref,*this,dispatch_table(),allocation_callbacks_);
  return descriptor_sets;
}
)",
    },
    {"device", "create_compute_pipelines",
     R"(
    inline std::vector<spk::pipeline>
    create_compute_pipelines(spk::pipeline_cache_ref pipeline_cache,
                              spk::array_view<const spk::compute_pipeline_create_info> create_infos);
    )",
     R"(
inline std::vector<spk::pipeline>
device::create_compute_pipelines(spk::pipeline_cache_ref pipeline_cache,
                         spk::array_view<const spk::compute_pipeline_create_info> create_infos) {
 std::vector<spk::pipeline_ref> pipeline_refs(create_infos.size());
 dispatch_table().create_compute_pipelines(handle_,pipeline_cache, create_infos.size(), create_infos.data(), allocation_callbacks_, pipeline_refs.data());
 std::vector<spk::pipeline> pipelines;
 pipelines.reserve(create_infos.size());
 for (auto pipeline_ref : pipeline_refs)
   pipelines.emplace_back(pipeline_ref,*this,dispatch_table(),allocation_callbacks_);
 return pipelines;
}
)"},
    {"device", "create_shared_swapchains_khr",
     R"(
inline std::vector<spk::swapchain_khr>
create_shared_swapchains_khr(spk::array_view<const spk::swapchain_create_info_khr> create_infos);
)",
     R"(
inline std::vector<spk::swapchain_khr>
device::create_shared_swapchains_khr(spk::array_view<const spk::swapchain_create_info_khr> create_infos) {
std::vector<spk::swapchain_khr_ref> swapchain_refs(create_infos.size());
dispatch_table().create_shared_swapchains_khr(handle_, create_infos.size(), create_infos.data(), allocation_callbacks_, swapchain_refs.data());
std::vector<spk::swapchain_khr> swapchains;
swapchains.reserve(create_infos.size());
for (auto swapchain_ref : swapchain_refs)
swapchains.emplace_back(swapchain_ref,*this,dispatch_table(),allocation_callbacks_);
return swapchains;
}
)"},
    {"device", "create_ray_tracing_pipelines_nv",
     R"(
inline std::vector<spk::pipeline>
create_ray_tracing_pipelines_nv(spk::pipeline_cache_ref pipeline_cache,
                          spk::array_view<const spk::ray_tracing_pipeline_create_info_nv> create_infos);
)",
     R"(
inline std::vector<spk::pipeline>
device::create_ray_tracing_pipelines_nv(spk::pipeline_cache_ref pipeline_cache,
                     spk::array_view<const spk::ray_tracing_pipeline_create_info_nv> create_infos) {
std::vector<spk::pipeline_ref> pipeline_refs(create_infos.size());
dispatch_table().create_ray_tracing_pipelines_nv(handle_,pipeline_cache, create_infos.size(), create_infos.data(), allocation_callbacks_, pipeline_refs.data());
std::vector<spk::pipeline> pipelines;
pipelines.reserve(create_infos.size());
for (auto pipeline_ref : pipeline_refs)
pipelines.emplace_back(pipeline_ref,*this,dispatch_table(),allocation_callbacks_);
return pipelines;
}
)"},
    {"swapchain_khr", "get_swapchain_images_khr",
     R"(
     inline std::vector<spk::image> images_khr();
    )",
     R"(
     inline std::vector<spk::image> swapchain_khr::images_khr()
     {
      uint32_t size_;
      spk::result success1_ = dispatch_table().get_swapchain_images_khr(
          parent_, handle_, &size_, nullptr);
      if (success1_ != spk::result::success)
        throw spk::unexpected_command_result(success1_,
        "vkGetSwapchainImagesKHR");
      std::vector<spk::image_ref> result_(size_);
      spk::result success2_ = dispatch_table().get_swapchain_images_khr(
          parent_, handle_, &size_, result_.data());
      if (success2_ != spk::result::success)
        throw spk::unexpected_command_result(success2_,
        "vkGetSwapchainImagesKHR");
      std::vector<spk::image> result2_;
      result2_.reserve(size_);
      for (auto ref : result_)
        result2_.emplace_back(ref, parent_, dispatch_table(),
                              allocation_callbacks_);
      return result2_;
    }
    )"},
    {"display_khr", "create_display_mode_khr",
     R"(inline spk::display_mode_khr create_display_mode_khr(
    spk::display_mode_create_info_khr const& pCreateInfo); )",

     R"(inline spk::display_mode_khr display_khr::create_display_mode_khr(
    spk::display_mode_create_info_khr const& pCreateInfo) {
  spk::display_mode_khr_ref result_;
  dispatch_table().create_display_mode_khr(parent_, handle_, &pCreateInfo,
                                           allocation_callbacks_, &result_);
  return {result_, parent_, dispatch_table(), allocation_callbacks_};
}
)"}

};
}  // namespace sps
namespace {

std::map<std::string, std::string> handle_parents = {
    {"instance", ""},
    {"device", ""},
    {"descriptor_update_template", "device"},
    {"display_khr", "physical_device"},
    {"buffer", "device"},
    {"surface_khr", "instance"},
    {"event", "device"},
    {"query_pool", "device"},
    {"image", "device"},
    {"pipeline_layout", "device"},
    {"pipeline", "device"},
    {"command_buffer", "device"}};

const sps::Handle* get_handle(const vks::Type* t) {
  if (t == nullptr) return nullptr;
  auto n = dynamic_cast<const sps::Name*>(t);
  if (!n) return nullptr;
  return dynamic_cast<const sps::Handle*>(n->entity);
};

sps::MemberFunction* classify_command(sps::Registry& sreg,
                                      const vks::Registry& vreg,
                                      const sps::Command* command) {
  sps::MemberFunction* clas = new sps::MemberFunction;
  clas->name = command->name;
  clas->command = command;

  CHECK(command->params.size() >= 1);
  std::string name = command->name;
  const sps::Handle* dispatch_handle = get_handle(command->params.at(0).stype);
  if (!dispatch_handle && manual_translation_commands.count(name))
    return nullptr;
  CHECK(dispatch_handle) << name;
  const sps::Handle* second_handle = nullptr;
  if (command->params.size() > 1) {
    second_handle = get_handle(command->params.at(1).stype);
    if (second_handle) {
      std::string p = dispatch_handle->fullname;
      std::string s = second_handle->fullname;
      if (handle_parents.count(s)) {
        if (handle_parents.at(s) != p) second_handle = nullptr;
      } else {
        handle_parents[s] = p;
      }
    }
  }

  clas->main_handle = second_handle ? second_handle : dispatch_handle;
  clas->parent_dispatch = second_handle;

  if (!dvc::startswith(name, "destroy_") && second_handle &&
      command->params.at(1).param->get_optional(0)) {
    second_handle = nullptr;
    clas->main_handle = dispatch_handle;
    clas->parent_dispatch = false;
  }

  for (const auto& manual_translation : sps::Registry::manual_translations)
    if (manual_translation.command == name) {
      CHECK(!manual_translation_commands.count(name)) << name;
      clas->manual_translation = manual_translation;
      return clas;
    }

  if (manual_translation_commands.count(name)) return nullptr;

  if (dvc::startswith(name, "destroy_")) {
    CHECK_EQ(name, "destroy_" + clas->main_handle->fullname);
    sps::Handle* handle = dvc::find_or_die(sreg.handles, clas->main_handle);
    CHECK(handle->destructor == nullptr)
        << name << " " << handle->destructor->name;
    handle->destructor = command;
    handle->destructor_parent = bool(second_handle);
    return nullptr;
  }

  std::vector<size_t> nonconst_params;
  for (size_t i = 0; i < command->params.size(); ++i) {
    const sps::Param& param = command->params.at(i);
    const vks::Type* type = sps::get_pointee(param.stype);
    if (!type) continue;
    if (dynamic_cast<const vks::Const*>(type)) continue;

    if (auto nm = dynamic_cast<const sps::Name*>(type)) {
      const sps::Entity* entity = nm->entity;
      (void)entity;
    } else {
      continue;
    }
    nonconst_params.push_back(i);
  }
  CHECK(nonconst_params.size() < 2) << name;
  if (!nonconst_params.empty()) {
    CHECK_EQ(nonconst_params.at(0), command->params.size() - 1) << name;
    const sps::Param& last_param = command->params.back();
    if (last_param.param->len) {
      if (command->params.at(command->params.size() - 2).name !=
          last_param.param->len.value())
        LOG(FATAL) << last_param.param->len.value() << " " << name;
      else {
        if (command->resultvec_incomplete(clas->sz, clas->res)) {
          clas->resultvec_incomplete = true;
        } else if (command->resultvec_void(clas->sz, clas->res)) {
          clas->resultvec_void = true;
        } else {
          LOG(FATAL) << "resultvec: " << name;
        }
      }
    } else {
      CHECK(!last_param.param->get_optional(0)) << name;
      CHECK(command->sreturn_type->to_string() == "void")
          << command->sreturn_type->to_string() << " " << name;
      clas->result = true;
      clas->res = sps::get_pointee(last_param.stype);
      CHECK(clas->res);
    }
  }

  if (auto result_handle = get_handle(clas->res)) {
    clas->result_handle = result_handle;
  }

  std::unordered_set<std::string> param_names;
  for (const sps::Param& param : command->params)
    param_names.insert(param.name);
  for (const sps::Param& param : command->params)
    if (param.param->len)
      if (!param_names.count(param.param->len.value()))
        LOG(ERROR) << name << " " << param.name << " "
                   << param.param->len.value();

  std::map<size_t, std::vector<size_t>> szptr_pairs;

  for (size_t i = clas->begin(); i < clas->end(); ++i)
    for (size_t j = clas->begin(); j < clas->end(); ++j) {
      if (i == j) continue;
      const vks::Param* a = command->params.at(i).param;
      const vks::Param* b = command->params.at(j).param;

      if (b->len && b->len.value() == a->name) szptr_pairs[i].push_back(j);
    }

  for (auto x : szptr_pairs) {
    std::string sztype = command->params.at(x.first).stype->to_string();
    if (sztype != "uint32_t" && sztype != "size_t" && sztype != "uint64_t") {
      continue;
    }
    if (x.second.size() != 1 || x.second.at(0) != x.first + 1) continue;
    clas->szptrs.insert(x.first);
  }

  return clas;
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

    if (scommand->successcodes.size() == 1) {
      CHECK(scommand->command->return_type->to_string() == "VkResult");
      auto e = new vks::Entity;
      e->name = "void";
      auto n = new vks::Name;
      n->entity = e;
      scommand->sreturn_type = n;
    }
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

  for (const sps::Command* command : sreg.commands) {
    sps::MemberFunction* clas = classify_command(sreg, vreg, command);
    if (!clas) continue;
    sps::Handle* handle = sreg.handle_map.at(clas->main_handle->handle);
    clas->name = translate_member_function_name(handle->fullname, clas->name);
    handle->member_functions.push_back(clas);
  }

  std::unordered_map<std::string, const sps::Handle*> handle_names;
  handle_names[""] = nullptr;
  for (sps::Handle* handle : sreg.handles)
    handle_names[handle->fullname] = handle;
  for (sps::Handle* handle : sreg.handles)
    if (handle_parents.count(handle->fullname))
      handle->parent = handle_names.at(handle_parents.at(handle->fullname));

  for (const sps::Handle* handle : sreg.handles) {
    if (handle->destructor == nullptr)
      LOG(ERROR) << "no destructor: " << handle->fullname;
  }

  for (sps::Handle* handle : sreg.handles) {
    std::sort(handle->member_functions.begin(), handle->member_functions.end(),
              [](const sps::MemberFunction* a, const sps::MemberFunction* b) {
                return a->name < b->name;
              });
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
