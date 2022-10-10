#include "helpers.h"

Rectangle rotated_scaled_AABB(const Rectangle &restangle, float rotation, float scale) {
    Vector2 half_size = (Vector2){ restangle.width * 0.5f, restangle.height * 0.5f };

    Vector2 center = (Vector2){ restangle.x + half_size.x, restangle.y + half_size.y };

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

bool check_collision(const Rectangle &r1, const Rectangle &r2) {
    if ((r1.x < (r2.x + r2.width) && (r1.x + r1.width) > r2.x) &&
        (r1.y < (r2.y + r2.height) && (r1.y + r1.height) > r2.y))
        return true;

    return false;
}

Rectangle get_collision_overlap(const Rectangle &r1, const Rectangle &r2) {
    Rectangle res{ 0 };

    if (check_collision(r1, r2)) {
        float dxx = std::abs(r1.x - r2.x);
        float dyy = std::abs(r1.y - r2.y);

        if (r1.x <= r2.x) {
            if (r1.y <= r2.y) {
                res.x = r2.x;
                res.y = r2.y;

                res.width = r1.width - dxx;
                res.height = r1.height - dyy;
            }
            else {
                res.x = r2.x;
                res.y = r1.y;

                res.width = r1.width - dxx;
                res.height = r2.height - dyy;
            }
        }
        else {
            if (r1.y <= r2.y) {
                res.x = r1.x;
                res.y = r2.y;

                res.width = r2.width - dxx;
                res.height = r1.height - dyy;
            }
            else {
                res.x = r1.x;
                res.y = r1.y;

                res.width = r2.width - dxx;
                res.height = r2.height - dyy;
            }
        }

        if (r1.width > r2.width) {
            if (res.width >= r2.width)
                res.width = r2.width;
        }
        else {
            if (res.width >= r1.width)
                res.width = r1.width;
        }

        if (r1.height > r2.height) {
            if (res.height >= r2.height)
                res.height = r2.height;
        }
        else {
           if (res.height >= r1.height)
               res.height = r1.height;
        }
    }

    return res;
}

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), 
        [](unsigned char c){ return std::tolower(c); }
    );

    return s;
}
