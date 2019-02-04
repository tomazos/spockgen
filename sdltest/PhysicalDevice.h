#pragma once

namespace spk {

class PhysicalDevice {
 public:
 private:
  vk::PhysicalDevice physical_device;
  vk::PhysicalDeviceProperties physical_device_properties;
};

}  // namespace spk
