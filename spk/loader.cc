#include "loader.h"

#include <dlfcn.h>
#include <glog/logging.h>
#include <memory>

namespace spk {

template <typename T>
struct strip_member_function;
template <class C, typename F>
struct strip_member_function<F C::*> {
  using type = F;
};

template <typename T>
using strip_member_function_t = typename strip_member_function<T>::type;

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
}  // namespace spk
