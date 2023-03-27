#pragma once

#include <cmath>
#include <string>
#include <algorithm>

const float unit_to_pixels = 16.0f;
const float pixels_to_unit = 1.0f / unit_to_pixels;

struct Vector2 {
    float x, y;
};

struct Rectangle {
    float x, y, width, height;
};

struct Color {
    uint8_t r, g, b, a;
};

Rectangle rotated_scaled_AABB(const Rectangle &rectangle, float rotation, float scale);

// These mimic the functionality from RayLib https://github.com/raysan5/raylib
bool check_collision(const Rectangle &r1, const Rectangle &r2);
Rectangle get_collision_overlap(const Rectangle &r1, const Rectangle &r2);

std::string to_lower(std::string s);

inline int sign(float x) {
    if (x == 0.0f)
        return 0;

    return (x > 0.0f) * 2 - 1;
}
