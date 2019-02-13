#pragma once

#include <memory>
#define VULKAN_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "spk/spock.h"

namespace spk {

struct Allocator {
  void* allocate(size_t size, size_t alignment,
                 spk::system_allocation_scope allocation_scope);
  void* reallocate(void* original, size_t size, size_t alignment,
                   system_allocation_scope allocation_scope);
  void free(void* memory);
  void notify_internal_allocation(
      size_t size, spk::internal_allocation_type allocation_type,
      spk::system_allocation_scope allocation_scope);
  void notify_internal_free(size_t size,
                            spk::internal_allocation_type allocation_type,
                            spk::system_allocation_scope allocation_scope);
};

template <typename Allocator>
struct allocator_traits {
  using allocator_type = Allocator;
  using allocator_ptr = allocator_type*;

  static void* allocate(void* user_data, size_t size, size_t alignment,
                        VkSystemAllocationScope allocation_scope) {
    return allocator_ptr(user_data)->allocate(
        size, alignment, spk::system_allocation_scope(allocation_scope));
  }
  static void* reallocate(void* user_data, void* original, size_t size,
                          size_t alignment,
                          VkSystemAllocationScope allocation_scope) {
    return allocator_ptr(user_data)->reallocate(
        original, size, alignment,
        spk::system_allocation_scope(allocation_scope));
  }
  static void free(void* user_data, void* memory) {
    allocator_ptr(user_data)->free(memory);
  }
  static void notify_internal_allocation(
      void* user_data, size_t size, VkInternalAllocationType allocation_type,
      VkSystemAllocationScope allocation_scope) {
    allocator_ptr(user_data)->notify_internal_allocation(
        size, spk::internal_allocation_type(allocation_type),
        spk::system_allocation_scope(allocation_scope));
  }
  static void notify_internal_free(void* user_data, size_t size,
                                   VkInternalAllocationType allocation_type,
                                   VkSystemAllocationScope allocation_scope) {
    allocator_ptr(user_data)->notify_internal_free(
        size, spk::internal_allocation_type(allocation_type),
        spk::system_allocation_scope(allocation_scope));
  }

  static spk::allocation_callbacks create_allocation_callbacks(
      allocator_ptr allocator) {
    spk::allocation_callbacks callbacks;
    callbacks.set_p_user_data(allocator);
    callbacks.set_pfn_allocation(allocate);
    callbacks.set_pfn_reallocation(reallocate);
    callbacks.set_pfn_free(free);
    callbacks.set_pfn_internal_allocation(notify_internal_allocation);
    callbacks.set_pfn_internal_free(notify_internal_free);
    return callbacks;
  }
};

template <typename Allocator>
spk::allocation_callbacks create_allocation_callbacks(Allocator* allocator) {
  return spk::allocator_traits<Allocator>::create_allocation_callbacks(
      allocator);
}

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
