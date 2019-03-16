#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <gflags/gflags.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <glm/glm.hpp>
#include <random>
#include <set>

#include <iostream>
#include "dvc/file.h"
#include "dvc/terminate.h"
#include "spk/loader.h"
#include "spk/spock.h"
#include "spkx/helpers.h"
// #include "spk/presenter.h"
#include "spkx/game.h"
// #include "spk/rendering.h"
#include "dvc/log.h"

namespace {

DEFINE_bool(trace_allocations, false, "trace vulkan allocations");
DEFINE_uint64(num_points, 100, "num of points");

struct Pipeline {
  //  spk::shader_module vertex_shader, fragment_shader;
  spk::pipeline_layout pipeline_layout;
  spk::render_pass render_pass;
  spk::pipeline pipeline;
};

struct UniformBufferObject {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
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

    auto adjust = [](float& pos, float& vel) {
      if (pos > 1 && vel > 0) vel = -vel;
      if (pos < -1 && vel < 0) vel = -vel;
    };
    adjust(pos.x, velocity.x);
    adjust(pos.y, velocity.y);
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

std::vector<spk::vertex_input_attribute_description>
get_vertex_input_attribute_descriptions() {
  std::vector<spk::vertex_input_attribute_description> result(2);
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
  DVC_FATAL("no compatible memory type found");
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

spk::pipeline create_pipeline(spk::device& device, spkx::presenter& presenter) {
  spkx::pipeline_config config;
  config.vertex_shader = "test/pointtest.vert.spv";
  config.fragment_shader = "test/pointtest.frag.spv";
  config.vertex_binding_descriptions.push_back(
      get_vertex_input_binding_description());
  config.vertex_attribute_descriptions =
      get_vertex_input_attribute_descriptions();
  config.topology = spk::primitive_topology::point_list;

  return spkx::create_pipeline(device, presenter, config);
}

// spk::command_buffer create_command_buffer(spk::device& device,
//                                          spkx::presenter& presenter,
//                                          spk::command_pool& command_pool,
//                                          spk::pipeline& pipeline,
//                                          spkx::canvas& canvas,
//                                          VertexBuffer& buffer) {
//  spk::command_buffer_allocate_info command_buffer_allocate_info;
//  command_buffer_allocate_info.set_command_pool(command_pool);
//  command_buffer_allocate_info.set_level(spk::command_buffer_level::primary);
//  command_buffer_allocate_info.set_command_buffer_count(1);
//
//  spk::command_buffer command_buffer = std::move(
//      device.allocate_command_buffers(command_buffer_allocate_info).at(0));
//
//  spk::command_buffer_begin_info command_buffer_begin_info;
//  command_buffer_begin_info.set_flags(
//      spk::command_buffer_usage_flags::simultaneous_use);
//  command_buffer.begin(command_buffer_begin_info);
//
//  command_buffer.end();
//  return command_buffer;
//}

// struct Commands {
//  spk::command_pool command_pool;
//  std::vector<spk::command_buffer> command_buffers;
//};

// Commands create_commands(spk::device& device, uint32_t graphics_queue_family,
//                         spk::pipeline& pipeline, spkx::presenter& presenter,
//                         std::vector<VertexBuffer>& buffers) {
//  spk::command_pool command_pool =
//      create_command_pool(device, graphics_queue_family);
//
//  std::vector<spk::command_buffer> command_buffers;
//  for (VertexBuffer& buffer : buffers)
//    for (spkx::canvas& canvas : presenter.canvases())
//      command_buffers.push_back(create_command_buffer(
//          device, presenter, command_pool, pipeline, canvas, buffer));
//
//  return {std::move(command_pool), std::move(command_buffers)};
//}

std::vector<VertexBuffer> create_vertex_buffers(
    size_t num_renderings, spk::physical_device& physical_device,
    spk::device& device) {
  std::vector<VertexBuffer> buffers;
  for (size_t i = 0; i < num_renderings; ++i)
    buffers.emplace_back(create_vertex_buffer(physical_device, device));
  for (VertexBuffer& buffer : buffers) buffer.map();
  return buffers;
}

struct PointTest : spkx::game {
  World world;
  spk::pipeline pipeline;
  std::vector<VertexBuffer> buffers;

  PointTest(int argc, char** argv)
      : spkx::game(argc, argv),
        world(FLAGS_num_points),
        pipeline(create_pipeline(device(), presenter())),
        buffers(create_vertex_buffers(num_renderings(), physical_device(),
                                      device())) {}

  void tick() override {}

  void prepare_rendering(
      spk::command_buffer& command_buffer, size_t rendering_index,
      spk::render_pass_begin_info& render_pass_begin_info) override {
    VertexBuffer& current_buffer = buffers[rendering_index];

    world.update(0.01, 0.1, 0.01);
    current_buffer.update(world);

    spk::clear_color_value clear_color_value;
    clear_color_value.set_float_32({0, 0, 0, 1});

    spk::clear_value clear_color;
    clear_color.set_color(clear_color_value);
    render_pass_begin_info.set_clear_values({&clear_color, 1});

    command_buffer.begin_render_pass(render_pass_begin_info,
                                     spk::subpass_contents::inline_);

    command_buffer.bind_pipeline(spk::pipeline_bind_point::graphics, pipeline);
    spk::buffer_ref buffer_ref = current_buffer.buffer;
    uint64_t offset = 0;
    command_buffer.bind_vertex_buffers(0, 1, &buffer_ref, &offset);
    command_buffer.draw(FLAGS_num_points, 1, 0, 0);
    command_buffer.end_render_pass();
  }

  void mouse_motion(const mouse_motion_event& event) override {
    float mouse_x = event.x;
    float mouse_y = event.y;
    world.set_mouse_pos(glm::vec2{2 * mouse_x / window_size().x - 1,
                                  2 * mouse_y / window_size().y - 1});
  };

  ~PointTest() {
    for (VertexBuffer& buffer : buffers) {
      buffer.unmap();
      device().free_memory(buffer.device_memory);
    }
  }
};

}  // namespace

int main(int argc, char** argv) {
  PointTest pointtest(argc, argv);
  pointtest.run();
}
