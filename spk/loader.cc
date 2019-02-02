#include "loader.h"

#include <dlfcn.h>
#include <glog/logging.h>
#include <memory>

namespace spk {

instance::instance(spk::loader& loader,
                   spk::instance_create_info const* create_info,
                   spk::allocation_callbacks const* allocation_callbacks)
    : handle_(loader.create_instance(create_info, allocation_callbacks)),
      dispatch_table_(
          load_instance_dispatch_table(loader.pvkGetInstanceProcAddr, handle_)),
      allocation_callbacks_(allocation_callbacks) {}

spk::device_ref create_device(
    spk::physical_device& physical_device,
    spk::device_create_info const* create_info,
    spk::allocation_callbacks const* allocation_callbacks) {
  spk::device_ref device;
  physical_device.dispatch_table().create_device(physical_device, create_info,
                                                 allocation_callbacks, &device);
  return device;
}

device_dispatch_table load_device_dispatch_table(
    spk::physical_device& physical_device, spk::device_ref device) {
  auto pvkGetDeviceProcAddr =
      (PFN_vkGetDeviceProcAddr)physical_device.dispatch_table()
          .get_instance_proc_addr(physical_device.dispatch_table().instance,
                                  "vkGetDeviceProcAddr");
  return load_device_dispatch_table(pvkGetDeviceProcAddr, device);
}

device::device(spk::physical_device& physical_device,
               spk::device_create_info const* create_info,
               spk::allocation_callbacks const* allocation_callbacks)
    : handle_(
          create_device(physical_device, create_info, allocation_callbacks)),
      dispatch_table_(load_device_dispatch_table(physical_device, handle_)),
      allocation_callbacks_(allocation_callbacks) {}

loader::loader() : handle(::dlopen("libvulkan.so", RTLD_NOW | RTLD_GLOBAL)) {
  CHECK(handle != nullptr) << "Unable to dlopen libvulkan.so because "
                           << ::dlerror();
  ::dlerror();  // clear
  pvkGetInstanceProcAddr =
      (PFN_vkGetInstanceProcAddr)dlsym(handle.get(), "vkGetInstanceProcAddr");
  char* error = ::dlerror();
  CHECK(error == NULL)
      << "Unable to dlsym vkGetInstanceProcAddr from libulkan.so because "
      << error;

  global_dispatch_table = load_global_dispatch_table(pvkGetInstanceProcAddr);
}

void loader::deleter::operator()(void* handle) {
  CHECK(::dlclose(handle) == 0)
      << "Unable to dlclose libvulkan.so because " << ::dlerror();
}

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

spk::instance_ref loader::create_instance(
    spk::instance_create_info const* pCreateInfo,
    spk::allocation_callbacks const* pAllocator) const {
  spk::instance_ref instance_ref;
  global_dispatch_table.create_instance(pCreateInfo, pAllocator, &instance_ref);
  return instance_ref;
}

}  // namespace spk
