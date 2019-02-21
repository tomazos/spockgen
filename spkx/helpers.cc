#include "spkx/helpers.h"

#include "dvc/file.h"

namespace spkx {

spk::shader_module create_shader(spk::device& device,
                                 const std::filesystem::path& path) {
  CHECK(exists(path)) << "file not found: " << path;
  std::string code = dvc::load_file(path);
  spk::shader_module_create_info create_info;
  create_info.set_code_size(code.size());
  create_info.set_p_code((uint32_t*)code.data());
  return device.create_shader_module(create_info);
}

spk::pipeline create_pipeline(spk::device& device, spkx::presenter& presenter,
                              const spkx::pipeline_config& config) {
  spk::shader_module vertex_shader =
      spkx::create_shader(device, config.vertex_shader);
  spk::shader_module fragment_shader =
      spkx::create_shader(device, config.fragment_shader);

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

  spk::pipeline_vertex_input_state_create_info vertex_input_info;

  vertex_input_info.set_vertex_binding_descriptions(
      {config.vertex_binding_descriptions.data(),
       config.vertex_binding_descriptions.size()});

  vertex_input_info.set_vertex_attribute_descriptions(
      {config.vertex_attribute_descriptions.data(),
       config.vertex_attribute_descriptions.size()});

  spk::pipeline_input_assembly_state_create_info input_assembly;
  input_assembly.set_topology(config.topology);
  input_assembly.set_primitive_restart_enable(false);

  spk::viewport viewport;
  viewport.set_width(presenter.extent().width());
  viewport.set_height(presenter.extent().height());
  viewport.set_max_depth(1);

  spk::rect_2d scissor;
  scissor.set_extent(presenter.extent());

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
  pipeline_info.set_render_pass(presenter.render_pass());
  pipeline_info.set_subpass(0);
  pipeline_info.set_p_multisample_state(&multisampling);

  return device.create_graphics_pipeline(VK_NULL_HANDLE, pipeline_info);
}

}  // namespace spkx
