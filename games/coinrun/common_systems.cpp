#include "common_systems.h"

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

    // Render
    for (size_t i = 0; i < render_entities.size(); i++) {
        Entity e = render_entities[i].second;

        auto const &sprite = c.get_component<Component_Sprite>(e);
        auto const &transform = c.get_component<Component_Transform>(e);

        // Relative
        Vector2 position = (Vector2){ transform.position.x + std::cos(transform.rotation) * sprite.position.x, transform.position.y + std::sin(transform.rotation) * sprite.position.y };
        float rotation = transform.rotation + sprite.rotation;
        float scale = transform.scale * sprite.scale;

        DrawTextureEx(sprite.texture, position, rotation, scale, sprite.tint);
    }
}
