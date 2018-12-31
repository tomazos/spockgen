#pragma once

#include <memory>
#define VULKAN_NO_PROTOTYPES
#include <vulkan/vulkan.h>

namespace spk {

class loader {
 public:
  loader();

  template<typename PFN>
  PFN get_instance_proc_addr(VkInstance instance, const char* pName);

 private:
  struct deleter {
    void operator()(void* handle);
  };
  std::unique_ptr<void, deleter> handle;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
};

template<typename PFN>
PFN loader::get_instance_proc_addr(VkInstance instance, const char* pName) {
  return (PFN) vkGetInstanceProcAddr(instance, pName);
}

struct global_dispatch_table {
  global_dispatch_table(loader& loader);

  PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
  PFN_vkCreateInstance vkCreateInstance;
  PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
  PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion;
};

}  // namespace spk
