#pragma once

#include "spokk_platform.h"

#include <vulkan/vulkan.h>  // for VK_SUCCESS

#define SPOKK_VK_CHECK(expr) ZOMBO_RETVAL_CHECK(VK_SUCCESS, expr)
