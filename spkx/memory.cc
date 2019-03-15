#include "spkx/memory.h"

namespace spkx {

memory_type_index find_compatible_memory_type(
    spk::physical_device& physical_device, uint32_t memory_type_bits,
    spk::memory_property_flags flags) {
  spk::physical_device_memory_properties memory_properties =
      physical_device.memory_properties();

  for (uint32_t i = 0; i < memory_properties.memory_type_count(); ++i) {
    if (!(memory_type_bits & (1 << i))) continue;
    if (memory_properties.memory_types()[i].property_flags() & flags) return i;
  }
  LOG(FATAL) << "no compatible memory type found";
}

spk::buffer create_buffer(spk::device& device, size_t size,
                          spk::buffer_usage_flags usage,
                          spk::sharing_mode sharing_mode) {
  spk::buffer_create_info buffer_create_info;
  buffer_create_info.set_size(size);
  buffer_create_info.set_usage(usage);
  buffer_create_info.set_sharing_mode(sharing_mode);
  return device.create_buffer(buffer_create_info);
}

spk::device_memory create_memory(spk::device& device, uint64_t allocation_size,
                                 spkx::memory_type_index memory_type_index) {
  spk::memory_allocate_info memory_allocate_info;
  memory_allocate_info.set_allocation_size(allocation_size);
  memory_allocate_info.set_memory_type_index(memory_type_index);
  return device.allocate_memory(memory_allocate_info);
}

}  // namespace spkx
