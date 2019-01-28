#include "loader.h"

#include <dlfcn.h>
#include <glog/logging.h>
#include <memory>

namespace spk {

loader::loader() : handle(::dlopen("libvulkan.so", RTLD_NOW | RTLD_GLOBAL)) {
  CHECK(handle != nullptr) << "Unable to dlopen libvulkan.so because "
                           << ::dlerror();
  ::dlerror();  // clear
  vkGetInstanceProcAddr =
      (PFN_vkGetInstanceProcAddr)dlsym(handle.get(), "vkGetInstanceProcAddr");
  char* error = ::dlerror();
  CHECK(error == NULL)
      << "Unable to dlsym vkGetInstanceProcAddr from libulkan.so because "
      << error;

  spk::visit_dispatch_table(
      global_dispatch_table,
      [&](spk::global_dispatch_table& t, auto mf, const char* name) {
        using PFN = strip_member_function_t<decltype(mf)>;
        (t.*mf) = get_instance_proc_addr<PFN>(VK_NULL_HANDLE, name);
      });
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

}  // namespace spk
