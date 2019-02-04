#pragma once

#include <gflags/gflags.h>

#include <vulkan/vulkan.h>
#include <string>
#include <vulkan/vulkan.hpp>

#include "Utils.h"
#include "Version.h"

DEFINE_int32(vkdevice, -1, "index of vulkan device to use");

namespace spk {

VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
              void* pUserData) {
  std::cout << "VK: " << pCallbackData->pMessage << std::endl;

  return VK_FALSE;
}

class Instance {
 public:
  SPK_DECL_IMMOVABLE(Instance);

  Instance(const std::string& application_name,
           spk::Version application_version, const std::string& engine_name,
           spk::Version engine_version, spk::Version api_version,
           const std::vector<std::string>& extensions,
           const std::vector<std::string>& layers) {
    auto to_c_str = [](const std::vector<std::string>& in) {
      std::vector<const char*> out;
      for (const std::string& s : in) out.push_back(s.c_str());
      return out;
    };
    std::vector<const char*> ppEnabledExtensionNames = to_c_str(extensions);
    std::vector<const char*> ppEnabledLayerNames = to_c_str(layers);
    vk::InstanceCreateInfo instance_create_info;
    vk::ApplicationInfo application_info;
    application_info.setPApplicationName(application_name.c_str());
    application_info.setApiVersion(api_version);
    application_info.setEngineVersion(engine_version);
    application_info.setApplicationVersion(application_version);
    application_info.setPEngineName(engine_name.c_str());
    instance_create_info.setPApplicationInfo(&application_info);
    instance_create_info.setEnabledExtensionCount(
        ppEnabledExtensionNames.size());
    instance_create_info.setPpEnabledExtensionNames(
        &ppEnabledExtensionNames[0]);
    instance_create_info.setEnabledLayerCount(ppEnabledLayerNames.size());
    instance_create_info.setPpEnabledLayerNames(&ppEnabledLayerNames[0]);
    instance = vk::createInstance(instance_create_info);

    vk::DebugUtilsMessengerCreateInfoEXT messenger_create_info;
    messenger_create_info.setMessageSeverity(
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo);
    messenger_create_info.setMessageType(
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
    messenger_create_info.setPfnUserCallback(debugCallback);
    messenger = instance.createDebugUtilsMessengerEXT(messenger_create_info);

    std::vector<vk::PhysicalDevice> physical_devices =
        instance.enumeratePhysicalDevices();

    size_t physical_device_index;
    if (physical_devices.empty())
      throw std::runtime_error("could not find any vulkan physical devices");
    else if (physical_devices.size() == 1)
      physical_device_index = 0;
    else if (FLAGS_vkdevice < 0 ||
             FLAGS_vkdevice > int32_t(physical_devices.size())) {
      std::cerr << "Select vulkan device:" << std::endl;
      for (size_t i = 0; i < physical_devices.size(); i++) {
        vk::PhysicalDevice& physical_device = physical_devices[i];
        vk::PhysicalDeviceProperties physical_device_properties =
            physical_device.getProperties();
        std::cerr << " `--vkdevice " << i << "` ... "
                  << physical_device_properties.deviceName << std::endl;
      }
      std::exit(EXIT_FAILURE);
    } else {
      physical_device_index = FLAGS_vkdevice;
    }

    physical_device = physical_devices.at(physical_device_index);

    //    vk::PhysicalDeviceFeatures features = physical_device.getFeatures();
    std::vector<vk::QueueFamilyProperties> queue_families =
        physical_device.getQueueFamilyProperties();

    size_t queue_family_index = 0;
    while (queue_family_index != queue_families.size()) {
      vk::QueueFamilyProperties& queue_family =
          queue_families.at(queue_family_index);
      if (queue_family.queueCount > 0 &&
          (queue_family.queueFlags & vk::QueueFlagBits::eGraphics))
        break;

      queue_family_index++;
    }
    if (queue_family_index == queue_families.size())
      throw std::runtime_error("Unable to find appropriate queue family");

    vk::DeviceQueueCreateInfo device_queue_create_info;
    device_queue_create_info.setQueueFamilyIndex(queue_family_index);
    device_queue_create_info.setQueueCount(1);
    float queue_priority = 1.0f;
    device_queue_create_info.setPQueuePriorities(&queue_priority);
    vk::PhysicalDeviceFeatures device_features;
    vk::DeviceCreateInfo device_create_info;
    device_create_info.setPQueueCreateInfos(&device_queue_create_info);
    device_create_info.setQueueCreateInfoCount(1);
    device_create_info.setPEnabledFeatures(&device_features);

    const std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    device_create_info.setEnabledExtensionCount(device_extensions.size());
    device_create_info.setPpEnabledExtensionNames(&device_extensions[0]);

    device_create_info.setEnabledLayerCount(ppEnabledLayerNames.size());
    device_create_info.setPpEnabledLayerNames(&ppEnabledLayerNames[0]);
    device = physical_device.createDevice(device_create_info);
    queue = device.getQueue(queue_family_index, 0);
  }

  ~Instance() {
    device.destroy();
    instance.destroy(messenger);
    instance.destroy();
  }

  VkInstance to_vk_instance() { return instance; }
  vk::Instance to_cpp_instance() { return instance; }

  vk::Instance instance;
  vk::PhysicalDevice physical_device;
  vk::Device device;
  vk::Queue queue;
  vk::DebugUtilsMessengerEXT messenger;
};

}  // namespace spk
