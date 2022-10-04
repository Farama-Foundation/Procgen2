#include "helpers.h"

Rectangle rotated_scaled_AABB(const Rectangle &rectangle, float rotation, float scale) {
    Vector2 half_size = (Vector2){ rectangle.width * 0.5f, rectangle.height * 0.5f };

    Vector2 center = (Vector2){ rectangle.x + half_size.x, rectangle.y + half_size.y };

    Vector2 lower = center;
    Vector2 upper = center;

    for (int x = -1; x <= 1; x += 2)
        for (int y = -1; y <= 1; y += 2) {
            float cos_rot = std::cos(rotation);
            float sin_rot = std::sin(rotation);

            Vector2 corner{ half_size.x * x * scale, half_size.y * y * scale };

            // Rotate corner to get offset
            Vector2 offset{ cos_rot * corner.x - sin_rot * corner.y, sin_rot * corner.x + cos_rot * corner.y };
            
            Vector2 point = (Vector2){ center.x + offset.x, center.y + offset.y };

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
