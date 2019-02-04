#include "loader.h"

#include <dlfcn.h>
#include <glog/logging.h>
#include <memory>

#include <SDL2/SDL_vulkan.h>

namespace spk {

instance::instance(spk::instance_ref handle, const spk::loader& loader,
                   spk::allocation_callbacks const* allocation_callbacks)
    : handle_(handle),
      dispatch_table_(
          load_instance_dispatch_table(loader.pvkGetInstanceProcAddr, handle_)),
      allocation_callbacks_(allocation_callbacks) {}

device_dispatch_table load_device_dispatch_table(
    spk::physical_device& physical_device, spk::device_ref device) {
  auto pvkGetDeviceProcAddr =
      (PFN_vkGetDeviceProcAddr)physical_device.dispatch_table()
          .get_instance_proc_addr(physical_device.dispatch_table().instance,
                                  "vkGetDeviceProcAddr");
  return load_device_dispatch_table(pvkGetDeviceProcAddr, device);
}

device::device(spk::device_ref handle, spk::physical_device& physical_device,
               spk::allocation_callbacks const* allocation_callbacks)
    : handle_(handle),
      dispatch_table_(load_device_dispatch_table(physical_device, handle_)),
      allocation_callbacks_(allocation_callbacks) {}

loader::loader(const char* path) {
  if (SDL_Vulkan_LoadLibrary(path) != 0) LOG(FATAL) << SDL_GetError();

  pvkGetInstanceProcAddr =
      (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();

  CHECK(pvkGetInstanceProcAddr);

  global_dispatch_table = load_global_dispatch_table(pvkGetInstanceProcAddr);
}

loader::~loader() { SDL_Vulkan_UnloadLibrary(); }

spk::version loader::instance_version() const {
  uint32_t v;
  global_dispatch_table.enumerate_instance_version(&v);
  return v;
}

std::vector<spk::layer_properties> loader::instance_layer_properties() const {
  uint32_t c;
  CHECK_EQ(
      global_dispatch_table.enumerate_instance_layer_properties(&c, nullptr),
      spk::result::success);
  std::vector<spk::layer_properties> v(c);
  CHECK_EQ(
      global_dispatch_table.enumerate_instance_layer_properties(&c, v.data()),
      spk::result::success);
  return v;
}

std::vector<spk::extension_properties> loader::instance_extension_properties()
    const {
  return instance_extension_properties_(nullptr);
}

std::vector<spk::extension_properties> loader::instance_extension_properties(
    const std::string& layer_name) const {
  return instance_extension_properties(layer_name.c_str());
}

std::vector<spk::extension_properties> loader::instance_extension_properties_(
    const char* layer_name) const {
  uint32_t c;
  CHECK_EQ(global_dispatch_table.enumerate_instance_extension_properties(
               layer_name, &c, nullptr),
           spk::result::success);
  std::vector<spk::extension_properties> v(c);
  CHECK_EQ(global_dispatch_table.enumerate_instance_extension_properties(
               layer_name, &c, v.data()),
           spk::result::success);
  return v;
}

spk::instance loader::create_instance(
    spk::instance_create_info const& create_info,
    spk::allocation_callbacks const* allocation_callbacks) const {
  spk::instance_ref instance_ref;
  global_dispatch_table.create_instance(&create_info, allocation_callbacks,
                                        &instance_ref);

  return {instance_ref, *this, allocation_callbacks};
}

}  // namespace spk
