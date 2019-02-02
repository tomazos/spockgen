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

  spk::instance instance(loader, &instance_create_info, nullptr);

  spk::debug_utils_messenger_ext_ref debug_utils_messenger_ext_ref =
      instance.create_debug_utils_messenger_ext(
          debug_utils_messenger_create_info, nullptr);

  spk::debug_utils_messenger_ext debug_utils_messenger_ext(
      debug_utils_messenger_ext_ref, instance, instance.dispatch_table(),
      nullptr);

  for (spk::physical_device_ref physical_device_ref :
       instance.enumerate_physical_devices()) {
    spk::physical_device physical_device(physical_device_ref,
                                         instance.dispatch_table(), nullptr);

    LOG(ERROR) << "PHYSICAL DEVICE: "
               << physical_device.get_physical_device_properties()
                      .device_name()
                      .data();

    spk::device_create_info device_create_info;
    spk::device device(physical_device, &device_create_info, nullptr);
  }
}
