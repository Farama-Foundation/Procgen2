#include "common_systems.h"

#include "helpers.h"

void System_Sprite_Render::update() {
    if (render_entities.size() != entities.size())
        render_entities.resize(entities.size());

    int index = 0;

    for (auto const &e : entities) {
        auto const &sprite = c.get_component<Component_Sprite>(e);

        render_entities[index] = std::make_pair(sprite.z, e);
        index++;
    }
    
    // Sort sprites
    std::sort(render_entities.begin(), render_entities.end(), [](const std::pair<float, Entity> &left, const std::pair<float, Entity> &right) {
        return left.first < right.first;
    });
}

void System_Sprite_Render::render(const Rectangle &camera_aabb, Sprite_Render_Mode mode) {
    // Render
    for (size_t i = 0; i < render_entities.size(); i++) {
        Entity e = render_entities[i].second;

        auto const &sprite = c.get_component<Component_Sprite>(e);
        auto const &transform = c.get_component<Component_Transform>(e);

        // Sorting relative to tile map system - negative is behind, positive in front
        if (mode == positive_z && sprite.z < 0.0f)
            continue;
        else if (mode == negative_z && sprite.z >= 0.0f)
            break;

        // Relative
        float cos_rot = std::cos(transform.rotation);
        float sin_rot = std::sin(transform.rotation);

        Vector2 offset{ cos_rot * sprite.position.x - sin_rot * sprite.position.y, sin_rot * sprite.position.x + cos_rot * sprite.position.y };

        Vector2 position = (Vector2){ transform.position.x + offset.x, transform.position.y + offset.y };
        float rotation = transform.rotation + sprite.rotation;
        float scale = transform.scale * sprite.scale;

        // Find sprite AABB rectangle
        Rectangle aabb = (Rectangle){ position.x, position.y, sprite.texture.width * pixels_to_unit, sprite.texture.height * pixels_to_unit };
        aabb = rotated_scaled_AABB(aabb, rotation, scale);

        // If visible
        if (CheckCollisionRecs(aabb, camera_aabb))
            DrawTextureEx(sprite.texture, (Vector2){ position.x * unit_to_pixels, position.y * unit_to_pixels }, rotation, scale * unit_to_pixels / sprite.texture.width, sprite.tint);
    }
}
