#include <gflags/gflags.h>
#include <glog/logging.h>
#include <functional>

#include "spk/loader.h"
#include "spk/spock.h"

#include <iostream>

namespace spk {
using debug_utils_messenger_callback =
    std::function<void(spk::debug_utils_message_severity_flags_ext,
                       spk::debug_utils_message_type_flags_ext,
                       const spk::debug_utils_messenger_callback_data_ext*)>;

}  // namespace spk

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT messageType,
               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
               void* pUserData) {
  auto debug_utils_messenger_callback =
      (spk::debug_utils_messenger_callback*)pUserData;
  (*debug_utils_messenger_callback)(
      (spk::debug_utils_message_severity_flags_ext)messageSeverity,
      (spk::debug_utils_message_type_flags_ext)messageType,
      (const spk::debug_utils_messenger_callback_data_ext*)pCallbackData);
  return VK_FALSE;
}

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  spk::loader loader;

  CHECK_EQ(loader.instance_version(), spk::version(1, 1, 0));

  for (const spk::layer_properties& layer_properties :
       loader.instance_layer_properties()) {
    LOG(ERROR) << layer_properties.layer_name().data() << " "
               << layer_properties.implementation_version() << " "
               << layer_properties.description().data();
  }

  for (const spk::extension_properties& extension_properties :
       loader.instance_extension_properties()) {
    LOG(ERROR) << extension_properties.extension_name().data() << " "
               << extension_properties.spec_version();
  }

  spk::debug_utils_messenger_create_info_ext debug_utils_messenger_create_info;
  debug_utils_messenger_create_info.set_message_severity(
      spk::debug_utils_message_severity_flags_ext::warning_ext |
      spk::debug_utils_message_severity_flags_ext::error_ext |
      spk::debug_utils_message_severity_flags_ext::verbose_ext |
      spk::debug_utils_message_severity_flags_ext::info_ext);
  debug_utils_messenger_create_info.set_message_type(
      spk::debug_utils_message_type_flags_ext::general_ext |
      spk::debug_utils_message_type_flags_ext::performance_ext |
      spk::debug_utils_message_type_flags_ext::validation_ext);
  debug_utils_messenger_create_info.set_pfn_user_callback(debug_callback);

  spk::debug_utils_messenger_callback debug_utils_messenger_callback =
      [](spk::debug_utils_message_severity_flags_ext severity,
         spk::debug_utils_message_type_flags_ext type,
         const spk::debug_utils_messenger_callback_data_ext* data) {
        LOG(ERROR) << data->message();
      };

  debug_utils_messenger_create_info.set_p_user_data(
      &debug_utils_messenger_callback);

  spk::instance_create_info instance_create_info;
  const char* layers[] = {"VK_LAYER_LUNARG_standard_validation"};
  instance_create_info.set_pp_enabled_layer_names({layers, 1});
  const char* extensions[] = {spk::ext_debug_utils_extension_name};
  instance_create_info.set_pp_enabled_extension_names({extensions, 1});

  instance_create_info.set_next(&debug_utils_messenger_create_info);

  spk::instance_ref instance_ref =
      loader.create_instance(&instance_create_info, nullptr);
  spk::instance_dispatch_table instance_dispatch_table;

  spk::visit_dispatch_table(
      instance_dispatch_table,
      [&](spk::instance_dispatch_table& t, auto mf, const char* name) {
        using PFN = spk::strip_member_function_t<decltype(mf)>;
        (t.*mf) = loader.get_instance_proc_addr<PFN>(instance_ref, name);
      });

  spk::debug_utils_messenger_ext_ref debug_utils_messenger_ext_ref;

  instance_dispatch_table.create_debug_utils_messenger_ext(
      instance_ref, &debug_utils_messenger_create_info, nullptr,
      &debug_utils_messenger_ext_ref);

  uint32_t physical_device_count;
  CHECK(spk::result::success ==
        instance_dispatch_table.enumerate_physical_devices(
            instance_ref, &physical_device_count, nullptr));
  std::vector<spk::physical_device_ref> physical_device_refs(
      physical_device_count);
  CHECK(spk::result::success ==
        instance_dispatch_table.enumerate_physical_devices(
            instance_ref, &physical_device_count, physical_device_refs.data()));
  spk::device_create_info device_create_info;
  device_create_info.set_pp_enabled_layer_names({layers, 1});
  spk::device_ref device_ref;
  instance_dispatch_table.create_device(
      physical_device_refs.at(0), &device_create_info, nullptr, &device_ref);

  spk::device_dispatch_table device_dispatch_table;

  auto pvkGetDeviceProcAddr =
      (PFN_vkGetDeviceProcAddr)instance_dispatch_table.get_instance_proc_addr(
          instance_ref, "vkGetDeviceProcAddr");
  CHECK(pvkGetDeviceProcAddr);

  spk::visit_dispatch_table(
      device_dispatch_table,
      [&](spk::device_dispatch_table& t, auto mf, const char* name) {
        using PFN = spk::strip_member_function_t<decltype(mf)>;
        (t.*mf) = (PFN)pvkGetDeviceProcAddr(device_ref, name);
      });

  device_dispatch_table.destroy_device(device_ref, nullptr);
  instance_dispatch_table.destroy_instance(instance_ref, nullptr);
}
