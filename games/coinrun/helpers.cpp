#include "helpers.h"

Rectangle rotated_scaled_AABB(const Rectangle &rectangle, float rotation, float scale) {
    Vector2 half_size = (Vector2){ rectangle.width * 0.5f, rectangle.height * 0.5f };

    Vector2 center = (Vector2){ rectangle.x + half_size.x, rectangle.y + half_size.y };

    Vector2 lower = center;
    Vector2 upper = center;

    for (int x = -1; x <= 1; x += 2)
        for (int y = -1; y <= 1; y += 2) {
            Vector2 point = (Vector2){ center.x + std::cos(rotation) * half_size.x * x * scale, std::sin(rotation) * half_size.y * y * scale };

            // Expand bounds
            if (point.x < lower.x)
                lower.x = point.x;

            if (point.y < lower.y)
                lower.y = point.y;

            if (point.x > upper.x)
                upper.x = point.x;

            if (point.y > upper.y)
                upper.y = point.y;
        }

    return (Rectangle){ lower.x, lower.y, upper.x - lower.x, upper.y - lower.y };
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), 
        [](unsigned char c){ return std::tolower(c); }
    );

    return s;
}
