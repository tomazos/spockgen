#include "loader.h"

#include <dlfcn.h>
#include <glog/logging.h>
#include <memory>

namespace spk {

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
  return global_dispatch_table.enumerate_instance_layer_properties();
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
  return global_dispatch_table.enumerate_instance_extension_properties(
      layer_name);
}

spk::instance_ref loader::create_instance(
    spk::instance_create_info const* pCreateInfo,
    spk::allocation_callbacks const* pAllocator) const {
  spk::instance_ref instance_ref;
  global_dispatch_table.create_instance(pCreateInfo, pAllocator, &instance_ref);
  return instance_ref;
}

spk::global_dispatch_table load_global_dispatch_table(
    PFN_vkGetInstanceProcAddr pvkGetInstanceProcAddr) {
  spk::global_dispatch_table global_dispatch_table;
  spk::visit_dispatch_table(
      global_dispatch_table,
      [pvkGetInstanceProcAddr](spk::global_dispatch_table& t, auto mf,
                               const char* name) {
        using PFN = strip_member_function_t<decltype(mf)>;
        (t.*mf) = (PFN)pvkGetInstanceProcAddr(VK_NULL_HANDLE, name);
      });
  return global_dispatch_table;
}

spk::instance_dispatch_table load_instance_dispatch_table(
    PFN_vkGetInstanceProcAddr pvkGetInstanceProcAddr,
    spk::instance_ref instance) {
  spk::instance_dispatch_table instance_dispatch_table;
  spk::visit_dispatch_table(
      instance_dispatch_table,
      [pvkGetInstanceProcAddr, instance](spk::instance_dispatch_table& t,
                                         auto mf, const char* name) {
        using PFN = spk::strip_member_function_t<decltype(mf)>;
        (t.*mf) = (PFN)pvkGetInstanceProcAddr(instance, name);
      });
  return instance_dispatch_table;
}

spk::device_dispatch_table load_device_dispatch_table(
    PFN_vkGetDeviceProcAddr pvkGetDeviceProcAddr, spk::device_ref device) {
  spk::device_dispatch_table device_dispatch_table;

  spk::visit_dispatch_table(
      device_dispatch_table,
      [pvkGetDeviceProcAddr, device](spk::device_dispatch_table& t, auto mf,
                                     const char* name) {
        using PFN = spk::strip_member_function_t<decltype(mf)>;
        (t.*mf) = (PFN)pvkGetDeviceProcAddr(device, name);
      });
  return device_dispatch_table;
}

}  // namespace spk
