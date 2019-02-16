#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <glm/glm.hpp>
#include <random>
#include <set>

#include "dvc/file.h"
#include "dvc/terminate.h"
#include "spk/loader.h"
#include "spk/spock.h"
#include "spkx/helpers.h"
#include "spkx/program.h"
#include "spkx/swapchain.h"

#include <iostream>

namespace {

DEFINE_bool(trace_allocations, false, "trace vulkan allocations");
DEFINE_uint64(num_frames, 2, "num of frames in flight");
DEFINE_uint64(num_points, 100, "num of points");

struct Pipeline {
  //  spk::shader_module vertex_shader, fragment_shader;
  spk::pipeline_layout pipeline_layout;
  spk::render_pass render_pass;
  spk::pipeline pipeline;
};

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;
};

struct PointMass {
  glm::vec2 pos;
  glm::vec3 color;
  glm::vec2 velocity;
  void update(float alpha, float beta, float gamma, glm::vec2 mouse_pos) {
    glm::vec2 dmouse = mouse_pos - pos;

    velocity += beta * dmouse;

    velocity *= 1 - gamma;

    pos += alpha * velocity;

    while (pos.x > 1) pos.x -= 2;
    while (pos.x < -1) pos.x += 2;
    while (pos.y > 1) pos.y -= 2;
    while (pos.y < -1) pos.y += 2;
  }
};

spk::vertex_input_binding_description get_vertex_input_binding_description() {
  spk::vertex_input_binding_description vertex_input_binding_description;
  vertex_input_binding_description.set_binding(0);
  vertex_input_binding_description.set_input_rate(
      spk::vertex_input_rate::vertex);
  vertex_input_binding_description.set_stride(sizeof(Vertex));
  return vertex_input_binding_description;
}

std::array<spk::vertex_input_attribute_description, 2>
get_vertex_input_attribute_descriptions() {
  std::array<spk::vertex_input_attribute_description, 2> result;
  result[0].set_binding(0);
  result[0].set_location(0);
  result[0].set_format(spk::format::r32g32_sfloat);
  result[0].set_offset(offsetof(Vertex, pos));
  result[1].set_binding(0);
  result[1].set_location(1);
  result[1].set_offset(offsetof(Vertex, color));
  result[1].set_format(spk::format::r32g32b32_sfloat);
  return result;
}

spk::buffer create_buffer(spk::device& device) {
  spk::buffer_create_info buffer_create_info;
  buffer_create_info.set_size(sizeof(Vertex) * FLAGS_num_points);
  buffer_create_info.set_usage(spk::buffer_usage_flags::vertex_buffer);
  buffer_create_info.set_sharing_mode(spk::sharing_mode::exclusive);
  return device.create_buffer(buffer_create_info);
}

uint32_t find_compatible_memory_type(spk::physical_device& physical_device,
                                     spk::buffer& buffer) {
  const spk::memory_requirements memory_requirements =
      buffer.memory_requirements();

  static constexpr spk::memory_property_flags flags =
      spk::memory_property_flags::host_visible |
      spk::memory_property_flags::host_coherent;

  spk::physical_device_memory_properties memory_properties =
      physical_device.memory_properties();
  for (uint32_t i = 0; i < memory_properties.memory_type_count(); ++i) {
    if (!(memory_requirements.memory_type_bits() & (1 << i))) continue;
    if (memory_properties.memory_types()[i].property_flags() & flags) return i;
  }
  LOG(FATAL) << "no compatible memory type found";
}

spk::device_memory create_memory(spk::physical_device& physical_device,
                                 spk::device& device, spk::buffer& buffer) {
  const spk::memory_requirements memory_requirements =
      buffer.memory_requirements();
  spk::memory_allocate_info memory_allocate_info;
  memory_allocate_info.set_allocation_size(memory_requirements.size());
  memory_allocate_info.set_memory_type_index(
      find_compatible_memory_type(physical_device, buffer));
  return device.allocate_memory(memory_allocate_info);
}

struct World {
  World(size_t num_points) : num_points(num_points), points(FLAGS_num_points) {
    rng.seed(std::random_device()());

    for (size_t i = 0; i < num_points; ++i) {
      points[i].pos = glm::vec2{normal(), normal()};
      points[i].color = glm::vec3{normal(), normal(), normal()};
      points[i].velocity = glm::vec2{normal(), normal()};
      points[i].update(0, 0, 0, glm::vec2{0, 0});
    }
  }

  void update(float alpha, float beta, float gamma) {
    for (auto& point : points) point.update(alpha, beta, gamma, mouse_pos);
  }

  float normal() { return normal_(rng); }

  void set_mouse_pos(glm::vec2 mouse_pos) { this->mouse_pos = mouse_pos; }

  glm::vec2 mouse_pos;
  std::normal_distribution<float> normal_;
  std::mt19937 rng;
  size_t num_points;
  std::vector<PointMass> points;
};

struct VertexBuffer {
  spk::buffer buffer;
  spk::device_memory device_memory;
  uint64_t size;
  void* data;

  void map() { device_memory.map_memory(0, size, data); }
  void unmap() { device_memory.unmap_memory(); }
  void update(const World& world) {
    Vertex* v = (Vertex*)data;
    for (size_t i = 0; i < world.num_points; ++i) {
      v[i].color = world.points[i].color;
      v[i].pos = world.points[i].pos;
    }
  }
};

VertexBuffer create_vertex_buffer(spk::physical_device& physical_device,
                                  spk::device& device) {
  spk::buffer buffer = create_buffer(device);
  const spk::memory_requirements memory_requirements =
      buffer.memory_requirements();
  spk::device_memory device_memory =
      create_memory(physical_device, device, buffer);
  buffer.bind_memory(device_memory, 0);

  return {std::move(buffer), std::move(device_memory),
          memory_requirements.size()};
}

spk::pipeline create_pipeline(spk::device& device, spkx::swapchain& swapchain) {
  spk::shader_module vertex_shader =
      spkx::create_shader(device, "test/pointtest.vert.spv");
  spk::shader_module fragment_shader =
      spkx::create_shader(device, "test/pointtest.frag.spv");

  spk::pipeline_shader_stage_create_info vertex_shader_stage_create_info;
  vertex_shader_stage_create_info.set_module(vertex_shader);
  vertex_shader_stage_create_info.set_name("main");
  vertex_shader_stage_create_info.set_stage(spk::shader_stage_flags::vertex);

  spk::pipeline_shader_stage_create_info fragment_shader_stage_create_info;
  fragment_shader_stage_create_info.set_module(fragment_shader);
  fragment_shader_stage_create_info.set_name("main");
  fragment_shader_stage_create_info.set_stage(
      spk::shader_stage_flags::fragment);

  spk::pipeline_shader_stage_create_info pipeline_shader_stage_create_infos[2] =
      {vertex_shader_stage_create_info, fragment_shader_stage_create_info};

  spk::vertex_input_binding_description binding_description =
      get_vertex_input_binding_description();
  std::array<spk::vertex_input_attribute_description, 2> binding_attributes =
      get_vertex_input_attribute_descriptions();

  spk::pipeline_vertex_input_state_create_info vertex_input_info;
  vertex_input_info.set_vertex_binding_descriptions({&binding_description, 1});
  vertex_input_info.set_vertex_attribute_descriptions(
      {binding_attributes.data(), 2});

  spk::pipeline_input_assembly_state_create_info input_assembly;
  input_assembly.set_topology(spk::primitive_topology::point_list);
  input_assembly.set_primitive_restart_enable(false);

  spk::viewport viewport;
  viewport.set_width(swapchain.extent().width());
  viewport.set_height(swapchain.extent().height());
  viewport.set_max_depth(1);

  spk::rect_2d scissor;
  scissor.set_extent(swapchain.extent());

  spk::pipeline_viewport_state_create_info viewport_state;
  viewport_state.set_viewports({&viewport, 1});
  viewport_state.set_scissors({&scissor, 1});

  spk::pipeline_rasterization_state_create_info rasterizer;
  rasterizer.set_polygon_mode(spk::polygon_mode::fill);
  rasterizer.set_line_width(1);
  rasterizer.set_cull_mode(spk::cull_mode_flags::back);
  rasterizer.set_front_face(spk::front_face::clockwise);

  spk::pipeline_multisample_state_create_info multisampling;
  multisampling.set_rasterization_samples(spk::sample_count_flags::n1);
  multisampling.set_min_sample_shading(1);

  spk::pipeline_color_blend_attachment_state color_blend_attachement;
  color_blend_attachement.set_color_write_mask(
      spk::color_component_flags::r | spk::color_component_flags::g |
      spk::color_component_flags::b | spk::color_component_flags::a);

  spk::pipeline_color_blend_state_create_info color_blend;
  color_blend.set_attachments({&color_blend_attachement, 1});

  spk::pipeline_layout_create_info pipeline_layout_create_info;

  spk::pipeline_layout pipeline_layout =
      device.create_pipeline_layout(pipeline_layout_create_info);

  spk::graphics_pipeline_create_info pipeline_info;
  pipeline_info.set_stages({pipeline_shader_stage_create_infos, 2});
  pipeline_info.set_p_vertex_input_state(&vertex_input_info);
  pipeline_info.set_p_input_assembly_state(&input_assembly);
  pipeline_info.set_p_viewport_state(&viewport_state);
  pipeline_info.set_p_rasterization_state(&rasterizer);
  pipeline_info.set_p_color_blend_state(&color_blend);
  pipeline_info.set_layout(pipeline_layout);
  pipeline_info.set_render_pass(swapchain.render_pass());
  pipeline_info.set_subpass(0);
  pipeline_info.set_p_multisample_state(&multisampling);

  return device.create_graphics_pipeline(VK_NULL_HANDLE, pipeline_info);
}

spk::semaphore create_semaphore(spk::device& device) {
  return device.create_semaphore(spk::semaphore_create_info{});
}

spk::fence create_signaled_fence(spk::device& device) {
  spk::fence_create_info fence_create_info;
  fence_create_info.set_flags(spk::fence_create_flags::signaled);
  return device.create_fence(fence_create_info);
}

spk::command_pool create_command_pool(spk::device& device,
                                      uint32_t graphics_queue_family) {
  spk::command_pool_create_info pool_info;
  pool_info.set_queue_family_index(graphics_queue_family);
  return device.create_command_pool(pool_info);
}

spk::command_buffer create_command_buffer(spk::device& device,
                                          spkx::swapchain& swapchain,
                                          spk::command_pool& command_pool,
                                          spk::pipeline& pipeline,
                                          spkx::frame& frame,
                                          VertexBuffer& buffer) {
  spk::command_buffer_allocate_info command_buffer_allocate_info;
  command_buffer_allocate_info.set_command_pool(command_pool);
  command_buffer_allocate_info.set_level(spk::command_buffer_level::primary);
  command_buffer_allocate_info.set_command_buffer_count(1);

  spk::command_buffer command_buffer = std::move(
      device.allocate_command_buffers(command_buffer_allocate_info).at(0));

  spk::command_buffer_begin_info command_buffer_begin_info;
  command_buffer_begin_info.set_flags(
      spk::command_buffer_usage_flags::simultaneous_use);
  command_buffer.begin(command_buffer_begin_info);

  spk::render_pass_begin_info render_pass_info;
  render_pass_info.set_render_pass(swapchain.render_pass());
  render_pass_info.set_framebuffer(frame.framebuffer());
  spk::rect_2d render_area;
  render_area.set_extent(swapchain.extent());
  render_pass_info.set_render_area(render_area);
  spk::clear_color_value clear_color_value;
  clear_color_value.set_float_32({0, 0, 0, 1});
  spk::clear_value clear_color;
  clear_color.set_color(clear_color_value);
  render_pass_info.set_clear_values({&clear_color, 1});
  command_buffer.begin_render_pass(render_pass_info,
                                   spk::subpass_contents::inline_);
  command_buffer.bind_pipeline(spk::pipeline_bind_point::graphics, pipeline);
  spk::buffer_ref buffer_ref = buffer.buffer;
  uint64_t offset = 0;
  command_buffer.bind_vertex_buffers(0, 1, &buffer_ref, &offset);
  command_buffer.draw(FLAGS_num_points, 1, 0, 0);
  command_buffer.end_render_pass();
  command_buffer.end();
  return command_buffer;
}

struct Commands {
  spk::command_pool command_pool;
  std::vector<spk::command_buffer> command_buffers;
};

Commands create_commands(spk::device& device, uint32_t graphics_queue_family,
                         spk::pipeline& pipeline, spkx::swapchain& swapchain,
                         std::vector<VertexBuffer>& buffers) {
  spk::command_pool command_pool =
      create_command_pool(device, graphics_queue_family);

  std::vector<spk::command_buffer> command_buffers;
  for (VertexBuffer& buffer : buffers)
    for (spkx::frame& frame : swapchain.frames())
      command_buffers.push_back(create_command_buffer(
          device, swapchain, command_pool, pipeline, frame, buffer));

  return {std::move(command_pool), std::move(command_buffers)};
}

void main_loop(spk::device& device, spkx::swapchain& swapchain,
               spk::queue& present_queue, spk::queue& graphics_queue,
               std::vector<spk::command_buffer>& command_buffers,
               std::vector<VertexBuffer>& buffers, World& world,
               int window_width, int window_height) {
  std::vector<spk::semaphore> image_available_semaphores;
  std::vector<spk::semaphore> render_finished_semaphores;
  std::vector<spk::fence> fences;

  size_t num_frames = FLAGS_num_frames;
  CHECK_EQ(num_frames, buffers.size());
  size_t num_images = command_buffers.size() / num_frames;
  size_t current_frame = 0;

  for (size_t i = 0; i < num_frames; i++) {
    image_available_semaphores.push_back(create_semaphore(device));
    render_finished_semaphores.push_back(create_semaphore(device));
    fences.push_back(create_signaled_fence(device));
  }

  size_t niterations = 0;
  size_t nnofence = 0;
  size_t nnoacquire = 0;
  size_t npresent = 0;

  uint32_t last_report = SDL_GetTicks();

loop:
  while (1) {
    if (SDL_GetTicks() > last_report + 1000) {
      last_report = SDL_GetTicks();
      LOG(ERROR) << "niterations " << niterations << " nnofence " << nnofence
                 << " nnoacquire " << nnoacquire << " npresent " << npresent;
      niterations = 0;
      nnofence = 0;
      nnoacquire = 0;
      npresent = 0;
    }
    niterations++;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_QUIT:
          goto done;
        case SDL_KEYDOWN:
          if (event.key.keysym.sym == SDLK_q) goto done;
          break;
        case SDL_MOUSEBUTTONDOWN:
          LOG(ERROR) << "SDL_MOUSEBUTTONDOWN " << int(event.button.button);
          break;
        case SDL_MOUSEBUTTONUP:
          LOG(ERROR) << "SDL_MOUSEBUTTONUP " << int(event.button.button);
          break;
        case SDL_MOUSEMOTION:
          LOG(ERROR) << "SDL_MOUSEMOTION x " << event.motion.x << " y "
                     << event.motion.y << " xrel " << event.motion.xrel
                     << " yrel " << event.motion.yrel;
          float mouse_x = event.motion.x;
          float mouse_y = event.motion.y;
          world.set_mouse_pos(glm::vec2{2 * mouse_x / window_width - 1,
                                        2 * mouse_y / window_height - 1});

          break;
      }
    }

    spk::semaphore_ref image_available_semaphore =
        image_available_semaphores[current_frame];
    spk::semaphore_ref render_finished_semaphore =
        render_finished_semaphores[current_frame];
    spk::fence_ref current_fence = fences[current_frame];
    VertexBuffer& current_buffer = buffers[current_frame];

    spk::result wait_for_fences_result =
        device.wait_for_fences({&current_fence, 1}, true /*wait_all*/, 0);
    switch (wait_for_fences_result) {
      case spk::result::success:
        break;
      case spk::result::timeout:
        nnofence++;
        goto loop;
      default:
        LOG(FATAL) << "unexpected result " << wait_for_fences_result
                   << " from wait_for_fences";
    }

    uint32_t image_index;
    spk::result acquire_next_image_result =
        swapchain.get().acquire_next_image_khr(0, image_available_semaphore,
                                               VK_NULL_HANDLE, image_index);

    switch (acquire_next_image_result) {
      case spk::result::success:
        npresent++;
        break;
      case spk::result::not_ready:
        nnoacquire++;
        goto loop;
      default:
        LOG(FATAL) << "acquire_next_image_khr returned "
                   << acquire_next_image_result;
    }

    world.update(0.01, 0.1, 0.01);
    current_buffer.update(world);

    device.reset_fences({&current_fence, 1});

    spk::submit_info submit_info;
    spk::pipeline_stage_flags wait_stages[] = {
        spk::pipeline_stage_flags::color_attachment_output};
    spk::semaphore_ref wait = image_available_semaphore;
    submit_info.set_wait_semaphores({&wait, 1});
    submit_info.set_wait_dst_stage_mask({wait_stages, 1});
    spk::command_buffer_ref buf =
        command_buffers.at(current_frame * num_images + image_index);
    submit_info.set_command_buffers({&buf, 1});
    spk::semaphore_ref signal = render_finished_semaphore;
    submit_info.set_signal_semaphores({&signal, 1});
    graphics_queue.submit({&submit_info, 1}, current_fence);

    spk::present_info_khr present_info;
    present_info.set_wait_semaphores({&signal, 1});
    spk::swapchain_khr_ref sc = swapchain.get();
    present_info.set_swapchains({&sc, 1});
    present_info.set_image_indices({&image_index, 1});
    present_queue.present_khr(present_info);
    current_frame = (current_frame + 1) % num_frames;
  }
done:

  for (spk::fence_ref fence : fences) {
    CHECK_EQ(spk::result::success,
             device.wait_for_fences({&fence, 1}, true /*wait_all*/,
                                    std::numeric_limits<uint64_t>::max()));
  }
  graphics_queue.wait_idle();
  present_queue.wait_idle();
  device.wait_idle();
}

constexpr size_t roundup(size_t x, size_t f) {
  size_t r = x % f;
  if (r == 0)
    return x;
  else
    return x - r + f;
}

struct AllocationHeader {
  size_t payload_size;
};
static_assert(sizeof(AllocationHeader) % alignof(AllocationHeader) == 0);

inline AllocationHeader* payload_to_header(void* payload) {
  return (AllocationHeader*)(((char*)payload) - sizeof(AllocationHeader));
}
inline void* header_to_payload(AllocationHeader* header) {
  return ((char*)header) + sizeof(AllocationHeader);
}

inline void* create_allocation(const size_t payload_size,
                               const size_t payload_alignment) {
  const size_t actual_alignment =
      std::max(payload_alignment, alignof(AllocationHeader));
  const size_t actual_size =
      roundup(sizeof(AllocationHeader) + payload_size, actual_alignment);
  AllocationHeader* header =
      (AllocationHeader*)std::aligned_alloc(actual_alignment, actual_size);
  if (header == nullptr) return nullptr;
  header->payload_size = payload_size;
  return header_to_payload(header);
}

struct Allocator {
  void* allocate(size_t payload_size, size_t payload_alignment,
                 spk::system_allocation_scope allocation_scope) {
    void* new_payload = create_allocation(payload_size, payload_alignment);

    LOG(ERROR) << "allocate size " << payload_size << " alignment "
               << payload_alignment << " allocation_scope " << allocation_scope
               << " ptr " << new_payload;
    return new_payload;
  }

  void* reallocate(void* original_payload, size_t payload_size,
                   size_t payload_alignment,
                   spk::system_allocation_scope allocation_scope) {
    void* new_payload = create_allocation(payload_size, payload_alignment);
    if (new_payload != nullptr && original_payload != nullptr) {
      AllocationHeader* original_header = payload_to_header(original_payload);
      std::memcpy(new_payload, original_payload,
                  std::min(original_header->payload_size, payload_size));
      std::free(original_header);
    }
    LOG(ERROR) << "reallocate original " << original_payload << " size "
               << payload_size << " alignment " << payload_alignment
               << " allocation_scope " << allocation_scope << " ptr "
               << new_payload;
    return new_payload;
  }

  void free(void* memory) {
    if (memory == nullptr) return;
    AllocationHeader* header = payload_to_header(memory);
    LOG(ERROR) << "free memory " << memory;
    std::free(header);
  }

  void notify_internal_allocation(
      size_t size, spk::internal_allocation_type allocation_type,
      spk::system_allocation_scope allocation_scope) {
    LOG(ERROR) << "notify_internal_allocation size " << size
               << " allocation_type " << allocation_type << " allocation_scope "
               << allocation_scope;
  }

  void notify_internal_free(size_t size,
                            spk::internal_allocation_type allocation_type,
                            spk::system_allocation_scope allocation_scope) {
    LOG(ERROR) << "notify_internal_free size " << size << " allocation_type "
               << allocation_type << " allocation_scope " << allocation_scope;
  }
};

}  // namespace

int main(int argc, char** argv) {
  spkx::program program(argc, argv);
  spkx::swapchain swapchain(program.physical_device(), program.window(),
                            program.surface(), program.device());

  spk::device& device = program.device();

  World world(FLAGS_num_points);

  std::vector<VertexBuffer> buffers;

  for (size_t i = 0; i < FLAGS_num_frames; ++i)
    buffers.emplace_back(
        create_vertex_buffer(program.physical_device(), device));

  for (VertexBuffer& buffer : buffers) buffer.map();

  spk::pipeline pipeline = create_pipeline(device, swapchain);

  Commands commands = create_commands(device, program.graphics_queue_family(),
                                      pipeline, swapchain, buffers);

  main_loop(device, swapchain, program.present_queue(),
            program.graphics_queue(), commands.command_buffers, buffers, world,
            program.window_size().x, program.window_size().y);

  for (VertexBuffer& buffer : buffers) {
    buffer.unmap();
    device.free_memory(buffer.device_memory);
  }
}
