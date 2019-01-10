#pragma once

#include "vks/vks.h"
#include "vks/vulkan_relaxng.h"

vks::Registry parse_registry(const vkr::start& start);
