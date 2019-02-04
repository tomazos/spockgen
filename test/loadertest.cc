#include <gflags/gflags.h>
#include <glog/logging.h>
#include <functional>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
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

std::vector<std::string> get_sdl_extensions(SDL_Window* window) {
  unsigned int count;

  if (!SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr))
    LOG(FATAL) << SDL_GetError();

  std::vector<const char*> extensions_cstr(count);

  if (!SDL_Vulkan_GetInstanceExtensions(window, &count, extensions_cstr.data()))
    LOG(FATAL) << SDL_GetError();

  std::vector<std::string> extensions(count);
  for (unsigned int i = 0; i < count; i++) extensions[i] = extensions_cstr[i];
  return extensions;
}

int main(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (SDL_Init(SDL_INIT_VIDEO) != 0) LOG(FATAL) << SDL_GetError();

  int num_video_drivers = SDL_GetNumVideoDrivers();

  LOG(ERROR) << "num_video_drivers = " << num_video_drivers;
  for (int i = 0; i < num_video_drivers; i++) {
    LOG(ERROR) << "DRIVER #" << i << " " << SDL_GetVideoDriver(i);
  }

  LOG(ERROR) << "CURRENT: " << SDL_GetCurrentVideoDriver();

  int num_displays = SDL_GetNumVideoDisplays();
  LOG(ERROR) << "num_displays = " << num_displays;
  for (int i = 0; i < num_displays; i++) {
    SDL_Rect rect;
    CHECK(!SDL_GetDisplayBounds(i, &rect));
    LOG(ERROR) << "DISPLAY #" << i << " " << rect.x << " " << rect.y << " "
               << rect.w << " " << rect.h;
  }

  spk::loader loader;

  SDL_Window* window = SDL_CreateWindow(
      "loadertest", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0,
      SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_VULKAN);
  if (!window) LOG(FATAL) << SDL_GetError();

  int w, h;
  SDL_GetWindowSize(window, &w, &h);
  LOG(ERROR) << "SDL SIZE: " << w << " " << h;

  const std::vector<std::string> extensions = get_sdl_extensions(window);

  for (auto s : extensions) LOG(ERROR) << "SDL EXTENSION: " << s;

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
  std::vector<const char*> cextensions = {spk::ext_debug_utils_extension_name};
  for (const std::string& extension : extensions)
    cextensions.push_back(extension.c_str());
  instance_create_info.set_pp_enabled_extension_names(
      {cextensions.data(), cextensions.size()});

  //  instance_create_info.set_next(&debug_utils_messenger_create_info);

  spk::instance instance = loader.create_instance(instance_create_info);

  spk::debug_utils_messenger_ext debug_utils_messenger_ext =
      instance.create_debug_utils_messenger_ext(
          debug_utils_messenger_create_info);

  spk::surface_khr_ref surface_ref;
  if (!SDL_Vulkan_CreateSurface(window, instance, &surface_ref))
    LOG(FATAL) << SDL_GetError();
  spk::surface_khr surface(surface_ref, instance, instance.dispatch_table(),
                           nullptr);

  std::vector<spk::physical_device> physical_devices =
      instance.enumerate_physical_devices();
  spk::physical_device& physical_device = physical_devices.at(0);
  std::vector<spk::queue_family_properties> queue_family_properties_vector =
      physical_device.queue_family_properties();

  for (const auto& queue_family_properties : queue_family_properties_vector) {
    LOG(ERROR) << "queue_family_properties.queue_flags() = "
               << queue_family_properties.queue_flags();
  }

  spk::bool32_t b;
  physical_device.surface_support_khr(0, surface, b);
  CHECK(b);

  spk::surface_capabilities_khr surface_capabilities =
      physical_device.surface_capabilities_khr(surface);

  LOG(ERROR) << "HEIGHT = " << surface_capabilities.current_extent().height();
  LOG(ERROR) << "WIDTH = " << surface_capabilities.current_extent().width();

  spk::device_create_info device_create_info;
  spk::device_queue_create_info device_queue_create_info;
  float x = 0.5;
  device_queue_create_info.set_queue_priorities({&x, 1});
  device_queue_create_info.set_queue_family_index(0);

  device_create_info.set_queue_create_infos({&device_queue_create_info, 1});

  spk::device device = physical_device.create_device(device_create_info);

  while (1) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
          goto done;
        case SDL_KEYDOWN:
          if (event.key.keysym.sym == SDLK_q) goto done;
          LOG(ERROR) << "KEYDOWN SCANCODE '"
                     << SDL_GetScancodeName(event.key.keysym.scancode)
                     << "' KEYCODE '" << SDL_GetKeyName(event.key.keysym.sym)
                     << "'";
          break;
        case SDL_KEYUP:
          LOG(ERROR) << "KEYUP";
          break;
      }
    }
    // TODO: show frame
  }
done:
  device.wait_idle();
}
