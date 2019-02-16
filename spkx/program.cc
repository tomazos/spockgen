#include <SDL2/SDL_vulkan.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <functional>
#include <set>

#include "dvc/terminate.h"
#include "program.h"

namespace {

void init_sdl() {
  CHECK_EQ(SDL_Init(SDL_INIT_VIDEO /*| SDL_INIT_AUDIO*/), 0) << SDL_GetError();
  CHECK_EQ(SDL_SetRelativeMouseMode(SDL_TRUE), 0) << SDL_GetError();
}

void shutdown_sdl() { SDL_Quit(); }

SDL_Window* create_window(const std::string& name) {
  SDL_Window* window = SDL_CreateWindow(
      name.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0,
      SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_VULKAN);
  CHECK(window) << SDL_GetError();
  return window;
}

void destroy_window(SDL_Window* window) { SDL_DestroyWindow(window); }

glm::ivec2 get_window_size(SDL_Window* window) {
  glm::ivec2 window_size;
  SDL_GetWindowSize(window, &window_size.x, &window_size.y);
  return window_size;
}

std::vector<std::string> get_sdl_extensions(SDL_Window* window) {
  unsigned int count;

  CHECK(SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr))
      << SDL_GetError();

  std::vector<const char*> extensions_cstr(count);

  CHECK(
      SDL_Vulkan_GetInstanceExtensions(window, &count, extensions_cstr.data()))
      << SDL_GetError();

  std::vector<std::string> extensions(count);
  for (unsigned int i = 0; i < count; i++) extensions[i] = extensions_cstr[i];
  return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT messageType,
               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
               void* pUserData) {
  switch (messageSeverity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
      VLOG(0) << pCallbackData->pMessage;
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
      LOG(INFO) << pCallbackData->pMessage;
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
      LOG(WARNING) << pCallbackData->pMessage;
      break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
      LOG(ERROR) << pCallbackData->pMessage;
      break;
    default:
      LOG(FATAL) << "Unexpected message severity: " << messageSeverity;
  };

  return VK_FALSE;
}

spk::debug_utils_messenger_create_info_ext
create_debug_utils_messenger_create_info() {
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

  return debug_utils_messenger_create_info;
}

spk::instance create_instance(
    spk::loader& loader, const std::vector<std::string>& extensions,
    bool validation, const spk::allocation_callbacks* allocation_callbacks) {
  spk::instance_create_info instance_create_info;
  std::vector<const char*> layers;
  if (validation) layers.push_back("VK_LAYER_LUNARG_standard_validation");
  instance_create_info.set_pp_enabled_layer_names(
      {layers.data(), layers.size()});
  std::vector<const char*> cextensions = {spk::ext_debug_utils_extension_name,
                                          spk::ext_debug_report_extension_name};
  for (const std::string& extension : extensions)
    cextensions.push_back(extension.c_str());
  instance_create_info.set_pp_enabled_extension_names(
      {cextensions.data(), cextensions.size()});

  spk::debug_utils_messenger_create_info_ext debug_utils_messenger_create_info =
      create_debug_utils_messenger_create_info();
  instance_create_info.set_next(&debug_utils_messenger_create_info);

  spk::instance instance =
      loader.create_instance(instance_create_info, allocation_callbacks);
  return instance;
}

spk::debug_utils_messenger_ext create_debug_utils_messenger(
    spk::instance& instance) {
  return instance.create_debug_utils_messenger_ext(
      create_debug_utils_messenger_create_info());
}

spk::surface_khr create_surface(SDL_Window* window, spk::instance& instance) {
  spk::surface_khr_ref surface_ref;
  if (!SDL_Vulkan_CreateSurface(window, instance, &surface_ref))
    LOG(FATAL) << SDL_GetError();
  return {surface_ref, instance, instance.dispatch_table(), nullptr};
}

bool has_physical_device_extensions(
    spk::physical_device& physical_device,
    const std::vector<std::string>& required_extensions) {
  std::set<std::string> available_extensions;
  for (const spk::extension_properties& extension :
       physical_device.enumerate_device_extension_properties(nullptr))
    available_extensions.insert(extension.extension_name().data());
  for (const std::string& required_extension : required_extensions)
    if (!available_extensions.count(required_extension)) return false;
  return true;
}

bool is_graphics_queue_family(spk::physical_device& physical_device,
                              const spk::queue_family_properties& properties) {
  return properties.queue_flags() & spk::queue_flags::graphics;
}

bool is_present_queue_family(spk::physical_device& physical_device,
                             uint32_t queue_family_index,
                             spk::surface_khr& surface,
                             const spk::queue_family_properties& properties) {
  spk::bool32_t is_supported;
  physical_device.surface_support_khr(queue_family_index, surface,
                                      is_supported);
  return is_supported;
}

bool is_physical_device_suitable(spk::physical_device& physical_device,
                                 spk::surface_khr& surface) {
  if (physical_device.properties().device_type() !=
          spk::physical_device_type::discrete_gpu ||
      !has_physical_device_extensions(physical_device,
                                      {spk::khr_swapchain_extension_name}) ||
      physical_device.surface_formats_khr(surface).empty() ||
      physical_device.surface_present_modes_khr(surface).empty())
    return false;

  bool found_graphics_queue_family = false;
  bool found_present_queue_family = false;
  uint32_t queue_family_index = 0;
  for (const auto& queue_family_properties :
       physical_device.queue_family_properties()) {
    if (is_graphics_queue_family(physical_device, queue_family_properties))
      found_graphics_queue_family = true;
    if (is_present_queue_family(physical_device, queue_family_index, surface,
                                queue_family_properties))
      found_present_queue_family = true;
    queue_family_index++;
  }
  return found_graphics_queue_family && found_present_queue_family;
}

spk::physical_device select_physical_device(spk::instance& instance,
                                            spk::surface_khr& surface) {
  std::vector<spk::physical_device> physical_devices =
      instance.enumerate_physical_devices();

  for (auto& physical_device : physical_devices)
    if (is_physical_device_suitable(physical_device, surface))
      return std::move(physical_device);

  LOG(FATAL) << "Unable to find suitable physical device.";
}

uint32_t select_queue_family(
    spk::physical_device& physical_device,
    std::function<bool(uint32_t index,
                       const spk::queue_family_properties& properties)>
        is_relevant_queue_family) {
  uint32_t index = 0;
  for (const auto& properties : physical_device.queue_family_properties()) {
    if (is_relevant_queue_family(index, properties)) return index;
    index++;
  }

  LOG(FATAL) << "Queue family not found.";
}

uint32_t select_graphics_queue_family(spk::physical_device& physical_device) {
  return select_queue_family(
      physical_device,
      [&](uint32_t index, const spk::queue_family_properties& properties) {
        return is_graphics_queue_family(physical_device, properties);
      });
}

uint32_t select_present_queue_family(spk::physical_device& physical_device,
                                     spk::surface_khr& surface) {
  return select_queue_family(
      physical_device,
      [&](uint32_t index, const spk::queue_family_properties& properties) {
        return is_present_queue_family(physical_device, index, surface,
                                       properties);
      });
}

spk::device create_device(spk::physical_device& physical_device,
                          uint32_t graphics_queue_family,
                          uint32_t present_queue_family) {
  spk::device_create_info create_info;

  bool same_queue_family = (graphics_queue_family == present_queue_family);

  spk::device_queue_create_info queue_create_info[2];
  queue_create_info[0].set_queue_family_index(graphics_queue_family);
  queue_create_info[1].set_queue_family_index(present_queue_family);
  float queue_priority = 1.0;
  queue_create_info[0].set_queue_priorities({&queue_priority, 1});
  queue_create_info[1].set_queue_priorities({&queue_priority, 1});

  create_info.set_queue_create_infos(
      {queue_create_info, same_queue_family ? 1u : 2u});

  spk::physical_device_features physical_device_features;
  create_info.set_p_enabled_features(&physical_device_features);

  const char* layers[] = {"VK_LAYER_LUNARG_standard_validation"};
  create_info.set_pp_enabled_layer_names({layers, 1});

  const char* extensions[] = {spk::khr_swapchain_extension_name};
  create_info.set_pp_enabled_extension_names({extensions, 1});

  return physical_device.create_device(create_info);
}

}  // namespace

namespace spkx {

program::global::global(int argc, char** argv) {
  google::InitGoogleLogging(argv[0]);
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  dvc::install_terminate_handler();
  google::InstallFailureFunction(dvc::log_stacktrace);
  init_sdl();
}

program::global::~global() {
  shutdown_sdl();
  gflags::ShutDownCommandLineFlags();
  google::ShutdownGoogleLogging();
}

program::program(int argc, char** argv)
    : global_(argc, argv),
      name_(argv[0]),
      window_(create_window(name_), destroy_window),
      instance_(create_instance(loader_, get_sdl_extensions(window_.get()),
                                true, nullptr)),
      debug_utils_messenger_(create_debug_utils_messenger(instance_)),
      surface_(create_surface(window_.get(), instance_)),
      physical_device_(select_physical_device(instance_, surface_)),
      graphics_queue_family_(select_graphics_queue_family(physical_device_)),
      present_queue_family_(
          select_present_queue_family(physical_device_, surface_)),
      device_(create_device(physical_device_, graphics_queue_family_,
                            present_queue_family_)),
      graphics_queue_(device_.queue(graphics_queue_family_, 0)),
      present_queue_(device_.queue(present_queue_family_, 0)) {}

glm::ivec2 program::window_size() { return get_window_size(window_.get()); }

}  // namespace spkx
