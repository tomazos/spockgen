#pragma once

#include <memory>
#define VULKAN_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "spk/spock.h"

namespace spk {

class loader {
 public:
  loader(const char* path = nullptr);
  loader(const loader&) = delete;
  loader(loader&&) = delete;
  ~loader();

  PFN_vkGetInstanceProcAddr pvkGetInstanceProcAddr;

  spk::version instance_version() const;

  std::vector<spk::layer_properties> instance_layer_properties() const;
  std::vector<spk::extension_properties> instance_extension_properties() const;
  std::vector<spk::extension_properties> instance_extension_properties(
      const std::string& layer_name) const;

  spk::instance create_instance(
      spk::instance_create_info const& create_info,
      spk::allocation_callbacks const* allocator = nullptr) const;

  const spk::global_dispatch_table& dispatch_table() const {
    return global_dispatch_table;
  }

 private:
  std::vector<spk::extension_properties> instance_extension_properties_(
      const char* layer_name) const;

  spk::global_dispatch_table global_dispatch_table;
};

}  // namespace spk
