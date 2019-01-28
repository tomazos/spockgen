#pragma once

#include <memory>
#define VULKAN_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "spk/spock.h"

namespace spk {

class loader {
 public:
  loader();

  template <typename PFN>
  PFN get_instance_proc_addr(VkInstance instance, const char* pName);

  spk::version instance_version() const;

  std::vector<spk::layer_properties> instance_layer_properties() const;
  std::vector<spk::extension_properties> instance_extension_properties() const;
  std::vector<spk::extension_properties> instance_extension_properties(
      const std::string& layer_name) const;

  spk::instance_ref create_instance(
      spk::instance_create_info const* create_info,
      spk::allocation_callbacks const* allocator) const;

 private:
  std::vector<spk::extension_properties> instance_extension_properties_(
      const char* layer_name) const;

  spk::global_dispatch_table global_dispatch_table;

  struct deleter {
    void operator()(void* handle);
  };
  std::unique_ptr<void, deleter> handle;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
};

template <typename PFN>
PFN loader::get_instance_proc_addr(VkInstance instance, const char* pName) {
  return (PFN)vkGetInstanceProcAddr(instance, pName);
}

}  // namespace spk
