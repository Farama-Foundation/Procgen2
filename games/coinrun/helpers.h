#pragma once

#include <raylib.h>
#include <cmath>
#include <string>
#include <algorithm>

const float unit_to_pixels = 32.0f;
const float pixels_to_unit = 1.0f / unit_to_pixels;

Rectangle rotated_scaled_AABB(const Rectangle &rectangle, float rotation, float scale);

std::string to_lower(std::string s);
