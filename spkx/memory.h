#pragma once

#include "spk/spock.h"

namespace spkx {

using memory_type_index = uint32_t;

spkx::memory_type_index find_compatible_memory_type(
    spk::physical_device& physical_device, uint32_t memory_type_bits,
    spk::memory_property_flags flags);

spk::buffer create_buffer(
    spk::device& device, size_t size, spk::buffer_usage_flags usage,
    spk::sharing_mode sharing_mode = spk::sharing_mode::exclusive);

spk::device_memory create_memory(spk::device& device, uint64_t allocation_size,
                                 spkx::memory_type_index memory_type_index);
}  // namespace spkx
