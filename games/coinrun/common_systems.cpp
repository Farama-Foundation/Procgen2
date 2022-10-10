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

void System_Sprite_Render::render(Sprite_Render_Mode mode) {
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
        Rectangle aabb = (Rectangle){ position.x, position.y, sprite.texture->width * pixels_to_unit, sprite.texture->height * pixels_to_unit };
        aabb = rotated_scaled_AABB(aabb, rotation, scale);

        // If visible
        gr.render_texture(sprite.texture, (Vector2){ position.x * unit_to_pixels, position.y * unit_to_pixels }, scale * unit_to_pixels / sprite.texture->width, 1.0f, sprite.flip_x);
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

std::pair<bool, bool> System_Agent::update(float dt, const std::shared_ptr<System_Hazard> &hazard, const std::shared_ptr<System_Goal> &goal, int action) {
    bool alive = true;
    bool achieved_goal = false;

    // Parameters
    const float max_jump = 16.0f;
    const float gravity = 14.0f;
    const float max_speed = 6.0f;
    const float mix = 12.0f;
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
            // Set currently occupied tiles to not collide
            tilemap->set_no_collide(transform.position.x, tilemap->get_height() - 1 - static_cast<int>(transform.position.y - 0.99f));
            tilemap->set_no_collide(transform.position.x, tilemap->get_height() - 1 - static_cast<int>(transform.position.y - 0.5f));

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

        // Update world collision
        world_collision.x = transform.position.x + collision.bounds.x;
        world_collision.y = transform.position.y + collision.bounds.y;

        if (delta_position.x != 0.0f)
            dynamics.velocity.x = 0.0f;
        
        if (agent.on_ground)
            dynamics.velocity.y = 0.0f;

        // Go through all hazards
        for (auto const &h : hazard->get_entities()) {
            auto const &hazard_transform = c.get_component<Component_Transform>(h);
            auto const &hazard_collision = c.get_component<Component_Collision>(h);

            // World space
            Rectangle hazard_world_collision{ hazard_transform.position.x + hazard_collision.bounds.x, hazard_transform.position.y + hazard_collision.bounds.y, hazard_collision.bounds.width, hazard_collision.bounds.height };

            if (check_collision(world_collision, hazard_world_collision)) {
                alive = false;

                break;
            }
        }

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
        gr.camera_position.x = transform.position.x * unit_to_pixels;
        gr.camera_position.y = (transform.position.y - 0.5f) * unit_to_pixels;

        // Animation cycle
        agent.t += agent.rate * dt;
        agent.t = std::fmod(agent.t, 1.0f);

        if (movement_x > 0.0f)
            agent.face_forward = true;
        else if (movement_x < 0.0f)
            agent.face_forward = false;
    }

    return std::make_pair(alive, achieved_goal);
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

        Vector2 position{ transform.position.x - 0.5f, transform.position.y - 2.0f };

        gr.render_texture(texture, (Vector2){ position.x * unit_to_pixels, position.y * unit_to_pixels }, unit_to_pixels / texture->width, 1.0f, !agent.face_forward);
    }
}

void System_Particles::init() {
    particle_texture.load("assets/misc_assets/iconCircle_white.png");
}

void System_Particles::update(float dt) {
    for (auto const &e : entities) {
        auto const &transform = c.get_component<Component_Transform>(e);
        auto &particles = c.get_component<Component_Particles>(e);

        int dead_index = -1;
    
        for (int i = 0; i < particles.particles.size(); i++) {
            Particle &p = particles.particles[i];

            p.life -= dt;

            if (p.life <= 0.0f)
                dead_index = i;
        }

        particles.spawn_timer += dt;

        // If time to spawn new particle
        if (particles.spawn_timer >= particles.spawn_time) {
            particles.spawn_timer = std::fmod(particles.spawn_timer, particles.spawn_time);

            Particle &p = particles.particles[dead_index];

            p.life = particles.lifespan;
            p.position = transform.position;
        }
    }
}

void System_Particles::render() {
    const float scale = 1.0f;
    const Color color{ 255, 255, 255, 127 };

    for (auto const &e : entities) {
        auto const &transform = c.get_component<Component_Transform>(e);
        auto const &particles = c.get_component<Component_Particles>(e);

        for (int i = 0; i < particles.particles.size(); i++) {
            const Particle &p = particles.particles[i];

        }
    }
}

