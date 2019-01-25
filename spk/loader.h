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

 private:
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
