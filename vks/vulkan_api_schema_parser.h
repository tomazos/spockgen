#pragma once

#include "vks/vulkan_api_schema.h"
#include "vks/vulkan_relaxng.h"

vks::Registry parse_registry(const vkr::start& start);
