#include "common_systems.h"

#include "tilemap.h"

#include "helpers.h"

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

            int frames_advance = animation.t * animation.rate;
            animation.t -= frames_advance / animation.rate; 
                
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

void System_Sprite_Render::render(Sprite_Render_Mode mode) {
    // Render
    for (size_t i = 0; i < render_entities.size(); i++) {
        Entity e = render_entities[i].second;

        auto const &sprite = c.get_component<Component_Sprite>(e);
        auto const &transform = c.get_component<Component_Transform>(e);

        if (sprite.texture == nullptr)
            continue;

        // Sorting relative to tile map system - negative is behind, positive in front
        if (mode == positive_z && sprite.z < 0.0f)
            continue;
        else if (mode == negative_z && sprite.z >= 0.0f)
            break;

        float scale = transform.scale * sprite.scale;

        // If visible
        gr.render_texture(sprite.texture, (Vector2){ (transform.position.x + sprite.position.x) * unit_to_pixels, (transform.position.y + sprite.position.y) * unit_to_pixels }, scale * unit_to_pixels / sprite.texture->width, 1.0f, sprite.flip_x);
    }
}

void System_Agent::init() {
    agent_texture.load("assets/kenney/Enemies/mouse_move.png");
}

bool System_Agent::update(float dt, const std::shared_ptr<System_Goal> &goal, int action) {
    bool alive = true;
    bool achieved_goal = false;

    // Get tile map system
    std::shared_ptr<System_Tilemap> tilemap = c.system_manager.get_system<System_Tilemap>();

    assert(entities.size() == 1); // Only one player

    for (auto const &e : entities) {
        auto &agent = c.get_component<Component_Agent>(e);

        // Set action
        agent.action = action;

        auto &transform = c.get_component<Component_Transform>(e);

        const auto &collision = c.get_component<Component_Collision>(e);

        int movement_x = agent.action / 3 - 1;
        int movement_y = movement_x ? 0 : -(agent.action % 3 - 1);

        if (movement_x) {
            Tile_ID id = tilemap->get(static_cast<int>(transform.position.x + movement_x), tilemap->get_height() - 1 - static_cast<int>(transform.position.y));
            if (id == empty) {
              transform.position.x = static_cast<int>(transform.position.x + movement_x) + 0.5f;
            }
        } else if (movement_y) {
            Tile_ID id = tilemap->get(static_cast<int>(transform.position.x), tilemap->get_height() - 1 - static_cast<int>(transform.position.y + movement_y));
            if (id == empty) {
              transform.position.y = static_cast<int>(transform.position.y + movement_y) + 0.5f;
            }
        }

         Rectangle world_collision{ transform.position.x + collision.bounds.x, transform.position.y + collision.bounds.y, collision.bounds.width, collision.bounds.height };
        // Go through all goals
        for (auto const &g : goal->get_entities()) {
            auto const &goal_transform = c.get_component<Component_Transform>(g);
            auto const &goal_collision = c.get_component<Component_Collision>(g);

            // World space
            Rectangle goal_world_collision{ goal_transform.position.x + goal_collision.bounds.x, goal_transform.position.y + goal_collision.bounds.y, goal_collision.bounds.width, goal_collision.bounds.height };

            if (check_collision(world_collision, goal_world_collision)) {
                achieved_goal = true;

                break;
            }
        }

        // Camera follows the agent
        if (tilemap->center_agent()) {
            gr.camera_position.x = transform.position.x * unit_to_pixels;
            gr.camera_position.y = transform.position.y * unit_to_pixels;
        }

        // Animation cycle
        // agent.t += agent.rate * dt;
        // agent.t = std::fmod(agent.t, 1.0f);

        if (movement_x > 0.0f)
            agent.face_forward = true;
        else if (movement_x < 0.0f)
            agent.face_forward = false;
    }

    return achieved_goal;
}

void System_Agent::render() {
    assert(entities.size() == 1); // Only one player

    for (auto const &e : entities) {
        auto const &agent = c.get_component<Component_Agent>(e);
        auto const &transform = c.get_component<Component_Transform>(e);

        float agent_scale = 1.0f;
        Vector2 agent_offset{ -0.5f, -0.5f };

        gr.render_texture(&agent_texture, (Vector2){ (transform.position.x + agent_offset.x) * unit_to_pixels, (transform.position.y + agent_offset.y) * unit_to_pixels }, unit_to_pixels / agent_texture.width * agent_scale, 1.0f, agent.face_forward);
    }
}
