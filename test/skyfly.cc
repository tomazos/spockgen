#define GLM_ENABLE_EXPERIMENTAL

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <gflags/gflags.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <png++/png.hpp>
#include <random>
#include <set>

#include <iostream>
#include "dvc/file.h"
#include "dvc/log.h"
#include "dvc/terminate.h"
#include "spk/loader.h"
#include "spk/spock.h"
#include "spkx/game.h"
#include "spkx/helpers.h"
#include "spkx/memory.h"

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
  glm::mat4 model = glm::mat4(1.0);
  glm::mat4 view = glm::mat4(1.0);
  glm::mat4 proj = glm::mat4(1.0);
};

spk::descriptor_set_layout create_descriptor_set_layout(spk::device& device) {
  spk::descriptor_set_layout_binding binding;
  binding.set_binding(0);
  binding.set_descriptor_type(spk::descriptor_type::uniform_buffer);
  binding.set_immutable_samplers({nullptr, 1});
  binding.set_stage_flags(spk::shader_stage_flags::vertex);
  spk::descriptor_set_layout_create_info create_info;
  create_info.set_bindings({&binding, 1});
  return device.create_descriptor_set_layout(create_info);
}

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
};

struct PointMass {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec3 velocity;
  void update(float alpha) {
    pos += alpha * velocity;

    auto adjust = [](float& pos, float& vel) {
      if (pos > 1 && vel > 0) vel = -vel;
      if (pos < -1 && vel < 0) vel = -vel;
    };
    adjust(pos.x, velocity.x);
    adjust(pos.y, velocity.y);
    adjust(pos.z, velocity.z);
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
  result[0].set_format(spk::format::r32g32b32_sfloat);
  result[0].set_offset(offsetof(Vertex, pos));
  result[1].set_binding(0);
  result[1].set_location(1);
  result[1].set_offset(offsetof(Vertex, color));
  result[1].set_format(spk::format::r32g32b32_sfloat);
  return result;
}

// spk::device_memory create_memory(spk::physical_device& physical_device,
//                                 spk::device& device, spk::buffer& buffer) {
//  const spk::memory_requirements memory_requirements =
//      buffer.memory_requirements();
//  spk::memory_allocate_info memory_allocate_info;
//  memory_allocate_info.set_allocation_size(memory_requirements.size());
//  memory_allocate_info.set_memory_type_index(spkx::find_compatible_memory_type(
//      physical_device, memory_requirements.memory_type_bits(),
//      spk::memory_property_flags::host_visible |
//          spk::memory_property_flags::host_coherent));
//  return device.allocate_memory(memory_allocate_info);
//}

struct Object {
  glm::vec3 pos;
  glm::vec3 fac;
  glm::vec3 vel;
  glm::vec3 up;
  float dir = 0;

  void normalize() {
    fac = glm::normalize(fac);
    up = glm::normalize(up - dot(up, fac) * fac);
  }
};

struct World {
  Object player;

  World(size_t num_points) : num_points(num_points), points(num_points) {
    rng.seed(std::random_device()());

    for (size_t i = 0; i < num_points; ++i) {
      points[i].pos = glm::vec3{normal(), normal(), normal()};
      points[i].color = glm::vec3{normal(), normal(), normal()};
      points[i].velocity = glm::vec3{normal(), normal(), normal()};
      points[i].update(0);
    }

    player.pos = {-20, -20, -20};
    player.fac = {20, 20, 20};
    player.vel = {0, 0, 0};
    player.up = {0, 1, 0};
    player.normalize();
  }

  void update(float alpha) {
    for (auto& point : points) point.update(alpha);

    player.vel += player.dir * player.fac * 0.001f;
    player.pos += player.vel;
  }

  float normal() { return normal_(rng); }

  void move_head(glm::vec2 headrel) { /*static constexpr float k = 0.01; */
    player.fac = glm::rotate(player.fac, headrel.x * -0.001f, player.up);
    player.normalize();
    player.fac = glm::rotate(player.fac, headrel.y * 0.001f,
                             glm::normalize(glm::cross(player.fac, player.up)));
    player.normalize();
  }

  void go_forward() { player.dir += 1; }
  void go_backward() { player.dir -= 0.2; }
  void go_left() { DVC_LOG("go_left"); }
  void go_right() { DVC_LOG("go_right"); }
  void stop_forward() { player.dir -= 1; }
  void stop_backward() { player.dir += 0.2; }
  void stop_left() { DVC_LOG("stop_left"); }
  void stop_right() { DVC_LOG("stop_right"); }

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
  spk::buffer buffer =
      spkx::create_buffer(device, sizeof(Vertex) * FLAGS_num_points,
                          spk::buffer_usage_flags::vertex_buffer);
  const spk::memory_requirements memory_requirements =
      buffer.memory_requirements();
  spk::device_memory device_memory = spkx::create_memory(
      device, memory_requirements.size(),
      spkx::find_compatible_memory_type(
          physical_device, memory_requirements.memory_type_bits(),
          spk::memory_property_flags::host_visible |
              spk::memory_property_flags::host_coherent));

  buffer.bind_memory(device_memory, 0);

  return {std::move(buffer), std::move(device_memory),
          memory_requirements.size()};
}

struct UniformBuffer {
  spk::buffer buffer;
  spk::device_memory device_memory;
  uint64_t size;
  void* data;

  void map() { device_memory.map_memory(0, size, data); }
  void unmap() { device_memory.unmap_memory(); }
  void update(const UniformBufferObject& ubo) { std::memcpy(data, &ubo, size); }
};

UniformBuffer create_uniform_buffer(spk::physical_device& physical_device,
                                    spk::device& device) {
  spk::buffer buffer =
      spkx::create_buffer(device, sizeof(UniformBufferObject),
                          spk::buffer_usage_flags::uniform_buffer);
  const spk::memory_requirements memory_requirements =
      buffer.memory_requirements();
  spkx::memory_type_index memory_type_index = spkx::find_compatible_memory_type(
      physical_device, memory_requirements.memory_type_bits(),
      spk::memory_property_flags::host_visible |
          spk::memory_property_flags::host_coherent);
  spk::device_memory device_memory = spkx::create_memory(
      device, memory_requirements.size(), memory_type_index);
  buffer.bind_memory(device_memory, 0);

  return {std::move(buffer), std::move(device_memory),
          memory_requirements.size()};
}

spk::pipeline_layout create_pipeline_layout(
    spk::device& device, spk::descriptor_set_layout& descriptor_set_layout) {
  spk::pipeline_layout_create_info create_info;
  spk::descriptor_set_layout_ref descriptor_set_layout_ref =
      descriptor_set_layout;
  create_info.set_set_layouts({&descriptor_set_layout_ref, 1});
  return device.create_pipeline_layout(create_info);
}

spk::pipeline create_pipeline(spk::device& device, spkx::presenter& presenter,
                              spk::pipeline_layout& pipeline_layout) {
  spkx::pipeline_config config;
  config.vertex_shader = "test/skyfly.vert.spv";
  config.fragment_shader = "test/skyfly.frag.spv";
  config.vertex_binding_descriptions.push_back(
      get_vertex_input_binding_description());
  config.vertex_attribute_descriptions =
      get_vertex_input_attribute_descriptions();
  config.topology = spk::primitive_topology::point_list;
  config.layout = pipeline_layout;

  return spkx::create_pipeline(device, presenter, config);
}

std::vector<VertexBuffer> create_vertex_buffers(
    size_t num_renderings, spk::physical_device& physical_device,
    spk::device& device) {
  std::vector<VertexBuffer> buffers;
  for (size_t i = 0; i < num_renderings; ++i)
    buffers.emplace_back(create_vertex_buffer(physical_device, device));
  for (VertexBuffer& buffer : buffers) buffer.map();
  return buffers;
}

std::vector<UniformBuffer> create_uniform_buffers(
    size_t num_renderings, spk::physical_device& physical_device,
    spk::device& device) {
  std::vector<UniformBuffer> buffers;
  for (size_t i = 0; i < num_renderings; ++i)
    buffers.emplace_back(create_uniform_buffer(physical_device, device));
  for (UniformBuffer& buffer : buffers) buffer.map();
  return buffers;
}

spk::descriptor_pool create_descriptor_pool(spk::device& device,
                                            uint32_t pool_size) {
  spk::descriptor_pool_create_info create_info;
  create_info.set_max_sets(pool_size);
  spk::descriptor_pool_size size;
  size.set_descriptor_count(pool_size);
  size.set_type(spk::descriptor_type::uniform_buffer);
  create_info.set_pool_sizes({&size, 1});
  return device.create_descriptor_pool(create_info);
}

std::vector<spk::descriptor_set> create_descriptor_sets(
    spk::device& device, spk::descriptor_pool& descriptor_pool,
    spk::descriptor_set_layout& layout, uint32_t num_descriptors) {
  spk::descriptor_set_allocate_info allocate_info;
  allocate_info.set_descriptor_pool(descriptor_pool);
  std::vector<spk::descriptor_set_layout_ref> layouts(num_descriptors, layout);
  allocate_info.set_set_layouts({layouts.data(), num_descriptors});
  return device.allocate_descriptor_sets(allocate_info);
}

struct ImageBuffer {
  spk::buffer buffer;
  spk::device_memory device_memory;
  uint64_t size;
};

// ImageBuffer create_image_buffer(spk::physical_device& physical_device,
//                                spk::device& device) {
//  png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel>> image(
//      "test/eso09323a.png");
//  LOG(FATAL) << image.get_width() << " " << image.get_height() << std::endl;
//
//  image.get_pixbuf();
//  spk::buffer buffer =
//      spkx::create_buffer(device, sizeof(Vertex) * FLAGS_num_points,
//                          spk::buffer_usage_flags::transfer_src);
//  const spk::memory_requirements memory_requirements =
//      buffer.memory_requirements();
//  spk::device_memory device_memory = spkx::create_memory(
//      device, sizeof(Vertex) * FLAGS_num_points,
//      spkx::find_compatible_memory_type(
//          physical_device, memory_requirements.memory_type_bits(),
//          spk::memory_property_flags::host_visible |
//              spk::memory_property_flags::host_coherent));
//  buffer.bind_memory(device_memory, 0);
//
//  return {std::move(buffer), std::move(device_memory),
//          memory_requirements.size()};
//}

struct SkyFly : spkx::game {
  World world;
  spk::descriptor_set_layout descriptor_set_layout;
  spk::pipeline_layout pipeline_layout;
  spk::pipeline pipeline;
  std::vector<VertexBuffer> vertex_buffers;
  std::vector<UniformBuffer> uniform_buffers;
  spk::descriptor_pool descriptor_pool;
  std::vector<spk::descriptor_set> descriptor_sets;

  SkyFly(int argc, char** argv)
      : spkx::game(argc, argv),
        world(FLAGS_num_points),
        descriptor_set_layout(create_descriptor_set_layout(device())),
        pipeline_layout(
            create_pipeline_layout(device(), descriptor_set_layout)),
        pipeline(create_pipeline(device(), presenter(), pipeline_layout)),
        vertex_buffers(create_vertex_buffers(num_renderings(),
                                             physical_device(), device())),
        uniform_buffers(create_uniform_buffers(num_renderings(),
                                               physical_device(), device())),
        descriptor_pool(create_descriptor_pool(device(), num_renderings())),
        descriptor_sets(create_descriptor_sets(device(), descriptor_pool,
                                               descriptor_set_layout,
                                               num_renderings())) {
    DVC_ASSERT_EQ(uniform_buffers.size(), descriptor_sets.size());
    DVC_ASSERT_EQ(uniform_buffers.size(), num_renderings());
    for (size_t i = 0; i < num_renderings(); i++) {
      spk::descriptor_buffer_info buffer_info;
      buffer_info.set_buffer(uniform_buffers[i].buffer);
      buffer_info.set_offset(0);
      buffer_info.set_range(sizeof(UniformBufferObject));
      spk::write_descriptor_set write;
      write.set_buffer_info({&buffer_info, 1});
      write.set_descriptor_type(spk::descriptor_type::uniform_buffer);
      write.set_dst_array_element(0);
      write.set_dst_binding(0);
      write.set_dst_set(descriptor_sets[i]);
      write.set_image_info({nullptr, 1});
      write.set_texel_buffer_view({nullptr, 1});
      device().update_descriptor_sets({&write, 1}, {nullptr, 0});
    }
  }

  void tick() override {}

  void prepare_rendering(
      spk::command_buffer& command_buffer, size_t rendering_index,
      spk::render_pass_begin_info& render_pass_begin_info) override {
    // std::terminate();
    VertexBuffer& current_buffer = vertex_buffers[rendering_index];
    UniformBuffer& uniform_buffer = uniform_buffers[rendering_index];

    world.update(0.01);
    current_buffer.update(world);

    UniformBufferObject ubo;
    ubo.view = glm::lookAt(
        world.player.pos, world.player.pos + world.player.fac, world.player.up);
    ubo.proj = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
    uniform_buffer.update(ubo);
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
    spk::descriptor_set_ref descriptor_set_ref =
        descriptor_sets.at(rendering_index);
    command_buffer.bind_descriptor_sets(spk::pipeline_bind_point::graphics,
                                        pipeline_layout, 0,
                                        {&descriptor_set_ref, 1}, {nullptr, 0});
    command_buffer.draw(FLAGS_num_points, 1, 0, 0);
    command_buffer.end_render_pass();
  }

  void mouse_motion(const mouse_motion_event& event) override {
    world.move_head(glm::vec2{event.xrel, event.yrel});
  };

  virtual void key_down(const keyboard_event& event) {
    if (event.keysym.sym == SDLK_q) shutdown();

    if (event.repeat) return;

    switch (event.keysym.sym) {
      case SDLK_w:
        world.go_forward();
        break;
      case SDLK_s:
        world.go_backward();
        break;
      case SDLK_a:
        world.go_left();
        break;
      case SDLK_d:
        world.go_right();
        break;
    }
  }

  virtual void key_up(const keyboard_event& event) {
    switch (event.keysym.sym) {
      case SDLK_w:
        world.stop_forward();
        break;
      case SDLK_s:
        world.stop_backward();
        break;
      case SDLK_a:
        world.stop_left();
        break;
      case SDLK_d:
        world.stop_right();
        break;
    }
  }

  ~SkyFly() {
    for (VertexBuffer& buffer : vertex_buffers) {
      buffer.unmap();
      device().free_memory(buffer.device_memory);
    }
    for (UniformBuffer& buffer : uniform_buffers) {
      buffer.unmap();
      device().free_memory(buffer.device_memory);
    }
  }
};

}  // namespace

int main(int argc, char** argv) {
  SkyFly skyfly(argc, argv);
  skyfly.run();
}
