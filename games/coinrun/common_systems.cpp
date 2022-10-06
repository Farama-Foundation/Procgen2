#include "common_systems.h"

#include "tilemap.h"

#include "helpers.h"

#include <iostream>

void System_Sprite_Render::update(float dt) {
    if (render_entities.size() != entities.size())
        render_entities.resize(entities.size());

    int index = 0;

    for (auto const &e : entities) {
        auto &sprite = c.get_component<Component_Sprite>(e);

        // If also has animation
        if (c.entity_manager.get_signature(e)[c.component_manager.get_component_type<Component_Animation>()]) {
            // Has animation component
            auto &animation = c.get_component<Component_Animation>(e);

            animation.t += dt;

            int frames_advance = animation.t / animation.rate;
            animation.t -= frames_advance * animation.rate; 
                
            animation.frame_index = (animation.frame_index + frames_advance) % animation.frames.size();

            sprite.texture = animation.frames[animation.frame_index];
        }

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
        if (CheckCollisionRecs(aabb, camera_aabb)) {
            if (sprite.flip_x) {
                Rectangle source{ static_cast<float>(sprite.texture.width), 0.0f, -static_cast<float>(sprite.texture.width), static_cast<float>(sprite.texture.height) };
                Rectangle dest{ position.x * unit_to_pixels, position.y * unit_to_pixels, scale * unit_to_pixels, scale * unit_to_pixels * static_cast<float>(sprite.texture.height) / static_cast<float>(sprite.texture.width) };

                DrawTexturePro(sprite.texture, source, dest, (Vector2){ 0.0f, 0.0f }, rotation, sprite.tint);
            }
            else
                DrawTextureEx(sprite.texture, (Vector2){ position.x * unit_to_pixels, position.y * unit_to_pixels }, rotation, scale * unit_to_pixels / sprite.texture.width, sprite.tint);
        }
    }
}

void System_Mob_AI::update(float dt) {
    // Get tile map system
    std::shared_ptr<System_Tilemap> tilemap = c.system_manager.get_system<System_Tilemap>();

    for (auto const &e : entities) {
        auto &mob_ai = c.get_component<Component_Mob_AI>(e);

        auto &transform = c.get_component<Component_Transform>(e);

        // Move
        transform.position.x += mob_ai.velocity_x * dt;

        Rectangle wall_sensor{ transform.position.x - 0.5f, transform.position.y - 0.6f, 1.0f, 0.5f };
        Rectangle floor_sensor{ transform.position.x - 0.5f, transform.position.y + 0.6f, 1.0f, 0.5f };

        std::pair<Vector2, bool> wall_collision_data = tilemap->get_collision(wall_sensor, [](Tile_ID id) -> Collision_Type {
            return (id == wall_mid || id == wall_top ? full : none);
        });

        std::pair<Vector2, bool> floor_collision_data = tilemap->get_collision(floor_sensor, [](Tile_ID id) -> Collision_Type {
            return (id == empty ? full : none);
        });

        float new_x = wall_collision_data.first.x + 0.5f;

        if (floor_collision_data.second)
            new_x = floor_collision_data.first.x + 0.5f;

        float delta_x = new_x - transform.position.x;

        transform.position.x = new_x;

        if (wall_collision_data.second || floor_collision_data.second)
            mob_ai.velocity_x *= -1.0f; // Rebound

        // Flip sprite if needed
        auto &sprite = c.get_component<Component_Sprite>(e);

        sprite.flip_x = mob_ai.velocity_x > 0.0f;
    }
}

void System_Agent::init() {
    stand_textures.resize(agent_themes.size());
    jump_textures.resize(agent_themes.size());
    walk1_textures.resize(agent_themes.size());
    walk2_textures.resize(agent_themes.size());

    for (int i = 0; i < agent_themes.size(); i++) {
        stand_textures[i].load("assets/kenney/Players/128x256/" + agent_themes[i] + "/alien" + agent_themes[i] + "_stand.png");
        jump_textures[i].load("assets/kenney/Players/128x256/" + agent_themes[i] + "/alien" + agent_themes[i] + "_jump.png");
        walk1_textures[i].load("assets/kenney/Players/128x256/" + agent_themes[i] + "/alien" + agent_themes[i] + "_walk1.png");
        walk2_textures[i].load("assets/kenney/Players/128x256/" + agent_themes[i] + "/alien" + agent_themes[i] + "_walk2.png");
    }
}

void System_Agent::update(float dt, Camera2D &camera) {
    // Parameters
    const float max_jump = 8.0f;
    const float gravity = 8.0f;
    const float max_speed = 5.0f;
    const float mix = 10.0f;
    const float air_control = 0.15f;

    // Get tile map system
    std::shared_ptr<System_Tilemap> tilemap = c.system_manager.get_system<System_Tilemap>();

    assert(entities.size() == 1); // Only one player

    for (auto const &e : entities) {
        auto &agent = c.get_component<Component_Agent>(e);

        int new_action = 0;

        if (IsKeyDown(KEY_RIGHT))
            new_action = 0;
        else if (IsKeyDown(KEY_LEFT))
            new_action = 6;
        else
            new_action = 4;

        if (new_action != 4) {
            if (IsKeyDown(KEY_UP))
                new_action += 2;
            else if (IsKeyDown(KEY_DOWN))
                new_action += 0;
            else
                new_action += 1;
        }
        else {
            if (IsKeyDown(KEY_UP))
                new_action = 5;
            else if (IsKeyDown(KEY_DOWN))
                new_action = 3;
            else
                new_action = 4;
        }

        agent.action = new_action;

        auto &transform = c.get_component<Component_Transform>(e);
        auto &dynamics = c.get_component<Component_Dynamics>(e);

        const auto &collision = c.get_component<Component_Collision>(e);

        float movement_x = (agent.action == 0 || agent.action == 1) - (agent.action == 6 || agent.action == 7);
        bool jump = (agent.action == 2 || agent.action == 5 || agent.action == 8);
        bool fallthrough = (agent.action == 0 || agent.action == 3 || agent.action == 6);

        // Velocity control
        float mix_x = agent.on_ground ? mix : (mix * air_control);

        dynamics.velocity.x += mix_x * (max_speed * movement_x - dynamics.velocity.x) * dt;

        if (std::abs(dynamics.velocity.x) < mix_x * max_speed * dt)
            dynamics.velocity.x = 0.0f;

        if (jump && agent.on_ground)
            dynamics.velocity.y = -max_jump;
        else if (fallthrough) {
            // Set 1-2 tiles below to not collide
            tilemap->set_no_collide(transform.position.x - 0.49f, tilemap->get_height() - 1 - static_cast<int>(transform.position.y + 0.5f));
            tilemap->set_no_collide(transform.position.x + 0.49f, tilemap->get_height() - 1 - static_cast<int>(transform.position.y + 0.5f));
        }

        dynamics.velocity.y += gravity * dt;
        
        // Max fall speed is jump speed
        if (std::abs(dynamics.velocity.y) > max_jump)
            dynamics.velocity.y = (dynamics.velocity.y > 0.0f ? 1.0f : -1.0f) * max_jump;

        // Move
        transform.position.x += dynamics.velocity.x * dt;
        transform.position.y += dynamics.velocity.y * dt;

        // World space collision
        Rectangle world_collision{ transform.position.x + collision.bounds.x, transform.position.y + collision.bounds.y, collision.bounds.width, collision.bounds.height };

        std::pair<Vector2, bool> collision_data = tilemap->get_collision(world_collision, [](Tile_ID id) -> Collision_Type {
            return (id == wall_mid || id == wall_top ? full : (id == crate ? down_only : none));
        }, dynamics.velocity.y);

        // Update no collide mask (for fallthrough platform logic) given some large bounds to check around the agent
        tilemap->update_no_collide(world_collision, Rectangle{ transform.position.x - 4.0f, transform.position.y - 4.0f, 8.0f, 8.0f });

        // If was moved up, on ground
        Vector2 delta_position{ collision_data.first.x - world_collision.x, collision_data.first.y - world_collision.y };

        agent.on_ground = delta_position.y < 0.0f && collision_data.second;

        // Correct position
        transform.position.x = collision_data.first.x - collision.bounds.x;
        transform.position.y = collision_data.first.y - collision.bounds.y;

        if (delta_position.x != 0.0f)
            dynamics.velocity.x = 0.0f;
        
        if (delta_position.y != 0.0f)
            dynamics.velocity.y = 0.0f;

        // Camera follows the agent
        camera.target.x = transform.position.x * unit_to_pixels;
        camera.target.y = transform.position.y * unit_to_pixels;

        // Animation cycle
        agent.t += agent.rate * dt;
        agent.t = std::fmod(agent.t, 1.0f);

        if (movement_x > 0.0f)
            agent.face_forward = true;
        else if (movement_x < 0.0f)
            agent.face_forward = false;
    }
}

void System_Agent::render(int theme) {
    assert(entities.size() == 1); // Only one player

    for (auto const &e : entities) {
        auto const &agent = c.get_component<Component_Agent>(e);
        auto const &transform = c.get_component<Component_Transform>(e);
        auto const &dynamics = c.get_component<Component_Dynamics>(e);

        // Select the correct texture
        Texture2D texture;

        std::cout << agent.on_ground << std::endl;
        if (std::abs(dynamics.velocity.x) < 0.01f)
            texture = stand_textures[theme].texture;
        else if (!agent.on_ground) 
            texture = jump_textures[theme].texture;
        else if (agent.t > 0.5f)
            texture = walk2_textures[theme].texture;
        else
            texture = walk1_textures[theme].texture;

        float flip = agent.face_forward ? 1.0f : -1.0f;

        Vector2 position{ transform.position.x - 0.5f, transform.position.y - 2.0f };

        Rectangle source{ (agent.face_forward ? 0.0f : texture.width), 0.0f, flip * static_cast<float>(texture.width), static_cast<float>(texture.height) };
        Rectangle dest{ position.x * unit_to_pixels, position.y * unit_to_pixels, 1.0f * unit_to_pixels, 2.0f * unit_to_pixels };

        DrawTexturePro(texture, source, dest, (Vector2){ 0 }, 0.0f, WHITE);
    }
}
