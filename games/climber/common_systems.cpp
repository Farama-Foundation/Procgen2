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

void System_Point::update() {
    // get entities
    std::shared_ptr<System_Mob_AI> mob_ai = c.system_manager.get_system<System_Mob_AI>();

    // get agent
    std::shared_ptr<System_Agent> agent = c.system_manager.get_system<System_Agent>();

    Entity e_agent = *agent->entities.begin();

    const Component_Transform &agent_transform = c.get_component<Component_Transform>(e_agent);
    const Component_Collision &agent_collision = c.get_component<Component_Collision>(e_agent);

    Rectangle agent_rect = agent_collision.bounds;
    agent_rect.x += agent_transform.position.x;
    agent_rect.y += agent_transform.position.y;

    num_points_available = 0;
    point_delta = 0;

    std::vector<Entity> to_destroy;

    for (auto const &e : entities) {
        auto &transform = c.get_component<Component_Transform>(e);
        auto &collision = c.get_component<Component_Collision>(e);

        Rectangle rect = collision.bounds;
        rect.x += transform.position.x;
        rect.y += transform.position.y;

        if (check_collision(agent_rect, rect)) {
            num_points_collected++;
            point_delta++;

            to_destroy.push_back(e);
        }
        else
            num_points_available++;
    }

    for (int i = 0; i < to_destroy.size(); i++)
        c.destroy_entity(to_destroy[i]);
}

bool System_Mob_AI::update(float dt) {
    const float anim_time = 2.0f;

    bool player_hit = false;

    // Get tile map system
    std::shared_ptr<System_Tilemap> tilemap = c.system_manager.get_system<System_Tilemap>();
    std::shared_ptr<System_Agent> agent = c.system_manager.get_system<System_Agent>();

    Entity e_agent = *agent->entities.begin();

    const Component_Transform &agent_transform = c.get_component<Component_Transform>(e_agent);
    const Component_Collision &agent_collision = c.get_component<Component_Collision>(e_agent);

    Rectangle agent_rect = agent_collision.bounds;
    agent_rect.x += agent_transform.position.x;
    agent_rect.y += agent_transform.position.y;

    for (auto const &e : entities) {
        auto &mob_ai = c.get_component<Component_Mob_AI>(e);

        auto &transform = c.get_component<Component_Transform>(e);
        auto &collision = c.get_component<Component_Collision>(e);

        // Move
        transform.position.x += mob_ai.velocity_x * dt;

        Rectangle wall_sensor{ transform.position.x - 0.5f, transform.position.y - 0.6f, 1.0f, 0.5f };

        std::pair<Vector2, bool> wall_collision_data = tilemap->get_collision(wall_sensor, [](Tile_ID id) -> Collision_Type {
            return (id == wall_mid || id == wall_top ? full : none);
        });

        float new_x = wall_collision_data.first.x + 0.5f;

        transform.position.x = new_x;

        Rectangle rect = collision.bounds;
        rect.x += transform.position.x;
        rect.y += transform.position.y;

        if (check_collision(agent_rect, rect)) {
            // Player loses
            player_hit = true;
        }

        bool end_patrol = transform.position.x > mob_ai.spawn_x + patrol_range || transform.position.x < mob_ai.spawn_x - patrol_range ;

        if (wall_collision_data.second || end_patrol)
            mob_ai.velocity_x *= -1.0f; // Rebound

        // Choose texture sprite
        auto &sprite = c.get_component<Component_Sprite>(e);

        // Flip sprite if needed
        sprite.flip_x = mob_ai.velocity_x < 0.0f;
    }

    return player_hit;
}

void System_Agent::init() {
    stand_textures.resize(agent_themes.size());
    jump_textures.resize(agent_themes.size());
    walk1_textures.resize(agent_themes.size());
    walk2_textures.resize(agent_themes.size());

    for (int i = 0; i < agent_themes.size(); i++) {
        stand_textures[i].load("assets/platformer/player" + agent_themes[i] + "_stand.png");
        jump_textures[i].load("assets/platformer/player" + agent_themes[i] + "_walk4.png");
        walk1_textures[i].load("assets/platformer/player" + agent_themes[i] + "_walk1.png");
        walk2_textures[i].load("assets/platformer/player" + agent_themes[i] + "_walk2.png");
    }
}

void System_Agent::update(float dt, int action) {
    // Parameters
    const float max_jump = 1.55f;
    const float gravity = 0.2f;
    const float max_speed = 0.5f;
    const float mix = 0.2f;
    const float air_control = 0.15f;

    // Get tile map system
    std::shared_ptr<System_Tilemap> tilemap = c.system_manager.get_system<System_Tilemap>();

    assert(entities.size() == 1); // Only one player

    for (auto const &e : entities) {
        auto &agent = c.get_component<Component_Agent>(e);

        // Set action
        agent.action = action;

        auto &transform = c.get_component<Component_Transform>(e);
        auto &dynamics = c.get_component<Component_Dynamics>(e);

        const auto &collision = c.get_component<Component_Collision>(e);

        float movement_x = (agent.action == 6 || agent.action == 7 || agent.action == 8) - (agent.action == 0 || agent.action == 1 || agent.action == 2);
        bool jump = (agent.action == 2 || agent.action == 5 || agent.action == 8);

        // Velocity control
        float mix_x = agent.on_ground ? mix : (mix * air_control);

        dynamics.velocity.x += mix_x * (max_speed * movement_x - dynamics.velocity.x) * dt;

        if (std::abs(dynamics.velocity.x) < mix_x * max_speed * dt)
            dynamics.velocity.x = 0.0f;

        if (jump && agent.on_ground)
            dynamics.velocity.y = -max_jump;

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
            return (id == wall_mid || id == wall_top ? full : none);
        });

        // If was moved up, on ground
        Vector2 delta_position{ collision_data.first.x - world_collision.x, collision_data.first.y - world_collision.y };

        agent.on_ground = delta_position.y < 0.0f && collision_data.second;

        // Correct position
        transform.position.x = collision_data.first.x - collision.bounds.x;
        transform.position.y = collision_data.first.y - collision.bounds.y;

        // Update world collision
        world_collision.x = transform.position.x + collision.bounds.x;
        world_collision.y = transform.position.y + collision.bounds.y;

        if (delta_position.x != 0.0f)
            dynamics.velocity.x = 0.0f;
        
        if (agent.on_ground)
            dynamics.velocity.y = 0.0f;
        
        // Camera follows the agent
        gr.camera_position.y = (transform.position.y - 8 - 0.5f) * unit_to_pixels;

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
        Asset_Texture* texture;

        if (std::abs(dynamics.velocity.x) < 0.01f && agent.on_ground)
            texture = &stand_textures[theme];
        else if (!agent.on_ground) 
            texture = &jump_textures[theme];
        else if (agent.t > 0.5f)
            texture = &walk2_textures[theme];
        else
            texture = &walk1_textures[theme];

        Vector2 position{ transform.position.x - 0.5f, transform.position.y - 1.0f };

        gr.render_texture(texture, (Vector2){ position.x * unit_to_pixels, position.y * unit_to_pixels }, transform.scale * unit_to_pixels / texture->width, 1.0f, !agent.face_forward);
    }
}
