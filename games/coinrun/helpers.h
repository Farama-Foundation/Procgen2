#pragma once

#include <raylib.h>
#include <cmath>
#include <string>
#include <algorithm>

Rectangle rotated_scaled_AABB(const Rectangle &rectangle, float rotation, float scale);

std::string to_lower(std::string s);
