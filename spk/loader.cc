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
}

void loader::deleter::operator()(void* handle) {
  CHECK(::dlclose(handle) == 0)
      << "Unable to dlclose libvulkan.so because " << ::dlerror();
}
}  // namespace spk
