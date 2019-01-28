#include <stdlib.h>
#include <stdio.h>
#include <vulkan/vulkan.h>

VkBool32 VKAPI_PTR debug_utils_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT                  messageType,
    const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
    void*                                            pUserData) {
  printf("MSG: %s\n", pCallbackData->pMessage);
  return VK_FALSE;
}

int main() {
  VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info = {};
  debug_utils_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debug_utils_messenger_create_info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
  debug_utils_messenger_create_info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
  debug_utils_messenger_create_info.pfnUserCallback = debug_utils_messenger_callback;

  VkInstanceCreateInfo instance_create_info = {};
  instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_create_info.pNext = &debug_utils_messenger_create_info;
  const char* layers[] = {"VK_LAYER_LUNARG_standard_validation"};
  instance_create_info.ppEnabledLayerNames = layers;
  instance_create_info.enabledLayerCount = 1;
  const char* extensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
  instance_create_info.enabledExtensionCount = 1;
  instance_create_info.ppEnabledExtensionNames = extensions;
  VkInstance instance;
  if (VK_SUCCESS != vkCreateInstance(&instance_create_info, NULL, &instance))
    exit(EXIT_FAILURE);

  // load kCreateDebugUtilsMessengerEXT
  PFN_vkCreateDebugUtilsMessengerEXT pvkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  if (pvkCreateDebugUtilsMessengerEXT == NULL)
    exit(EXIT_FAILURE);

  VkDebugUtilsMessengerEXT debug_utils_messenger;
  if (VK_SUCCESS != pvkCreateDebugUtilsMessengerEXT(instance, &debug_utils_messenger_create_info, NULL, &debug_utils_messenger))
    exit(EXIT_FAILURE);

  // destroy instance
  vkDestroyInstance(instance, NULL);
}

