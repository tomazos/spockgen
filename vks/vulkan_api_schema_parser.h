#pragma once

#include "vulkanhpp/vulkan_api_schema.h"
#include "vulkanhpp/vulkan_relaxng.h"

vks::Registry parse_registry(const vkr::start& start);
