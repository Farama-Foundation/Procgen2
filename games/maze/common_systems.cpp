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

    // Parameters
    // const float max_jump = 0.92f;
    // const float gravity = 0.1f;
    // const float max_speed = 0.5f;
    // const float mix = 0.2f;
    // const float air_control = 1.0f;
    // const float jump_cooldown = 3.0f;

    // Get tile map system
    std::shared_ptr<System_Tilemap> tilemap = c.system_manager.get_system<System_Tilemap>();

    assert(entities.size() == 1); // Only one player

    for (auto const &e : entities) {
        auto &agent = c.get_component<Component_Agent>(e);

        // Set action
        agent.action = action;

        auto &transform = c.get_component<Component_Transform>(e);
        // auto &dynamics = c.get_component<Component_Dynamics>(e);

        const auto &collision = c.get_component<Component_Collision>(e);

        //float movement_x = (agent.action == 6 || agent.action == 7 || agent.action == 8) - (agent.action == 0 || agent.action == 1 || agent.action == 2);
        int movement_x = agent.action / 3 - 1;
        //float movement_y = (agent.action == 2 || agent.action == 5 || agent.action == 8) - (agent.action == 0 || agent.action == 3 || agent.action == 6);
        int movement_y = movement_x ? 0 : -(agent.action % 3 - 1);

        // Velocity control
        // float mix_x = agent.on_ground ? mix : (mix * air_control);

        // dynamics.velocity.x += mix_x * (max_speed * movement_x - dynamics.velocity.x) * dt;

        // if (std::abs(dynamics.velocity.x) < mix_x * max_speed * dt)
        //     dynamics.velocity.x = 0.0f;

        // if (agent.on_ground)
        //     agent.jumps_left = 2;

        // if (jump && agent.jumps_left > 0 && agent.jump_timer == 0.0f) {
        //     dynamics.velocity.y = -max_jump;
        //     agent.jumps_left--;
        //     agent.jump_timer = jump_cooldown;
        // }

        // if (agent.jump_timer > 0.0f)
        //     agent.jump_timer = std::max(0.0f, agent.jump_timer - dt);

        // dynamics.velocity.y += gravity * dt;

        // Max fall speed is jump speed
        // if (std::abs(dynamics.velocity.y) > max_jump)
        //     dynamics.velocity.y = (dynamics.velocity.y > 0.0f ? 1.0f : -1.0f) * max_jump;

        // Move
        // transform.position.x += dynamics.velocity.x * dt;
        // transform.position.y += dynamics.velocity.y * dt;
        // transform.position.x += movement_x;
        // transform.position.y += movement_y;

        // // World space collision
        // Rectangle world_collision{ transform.position.x + collision.bounds.x, transform.position.y + collision.bounds.y, collision.bounds.width, collision.bounds.height };

        // std::pair<Vector2, bool> collision_data = tilemap->get_collision(world_collision, [](Tile_ID id) -> Collision_Type {
        //     return (id == wall || id == wall ? full : none);
        // // }, false, dynamics.velocity.y * dt);
        // }, false, 1);

        // // If was moved up, on ground
        // Vector2 delta_position{ collision_data.first.x - world_collision.x, collision_data.first.y - world_collision.y };

        // // agent.on_ground = delta_position.y < 0.0f && collision_data.second;

        // // Correct position
        // transform.position.x = collision_data.first.x - collision.bounds.x;
        // transform.position.y = collision_data.first.y - collision.bounds.y;

        // Update world collision
        // world_collision.x = transform.position.x + collision.bounds.x;
        // world_collision.y = transform.position.y + collision.bounds.y;

        // if (delta_position.x != 0.0f)
        //     dynamics.velocity.x = 0.0f;

        // If hit ceiling
        // if (delta_position.y > 0.0f && collision_data.second)
        //     dynamics.velocity.y = 0.0f;

        // if (agent.on_ground)
        //     dynamics.velocity.y = 0.0f;
        // if (delta_position.y != 0.0f)
        //     dynamics.velocity.y = 0.0f;

        // Go through all hazards
        // for (auto const &h : hazard->get_entities()) {
        //     auto const &hazard_transform = c.get_component<Component_Transform>(h);
        //     auto const &hazard_collision = c.get_component<Component_Collision>(h);

        //     // World space
        //     Rectangle hazard_world_collision{ hazard_transform.position.x + hazard_collision.bounds.x, hazard_transform.position.y + hazard_collision.bounds.y, hazard_collision.bounds.width, hazard_collision.bounds.height };

        //     if (check_collision(world_collision, hazard_world_collision)) {
        //         alive = false;

        //         break;
        //     }
        // }

        if (movement_x) {
            // Vector2 next_position{transform.position.x + movement_x, transform.position.y + movement_y};
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
        // gr.camera_position.x = transform.position.x * unit_to_pixels;
        // gr.camera_position.y = (transform.position.y - 0.5f) * unit_to_pixels;

        // Animation cycle
        // agent.t += agent.rate * dt;
        // agent.t = std::fmod(agent.t, 1.0f);

        if (movement_x > 0.0f)
            agent.face_forward = true;
        else if (movement_x < 0.0f)
            agent.face_forward = false;
    }

    // return std::make_pair(alive, achieved_goal);
    return achieved_goal;
}

void System_Agent::render() {
    assert(entities.size() == 1); // Only one player

    for (auto const &e : entities) {
        auto const &agent = c.get_component<Component_Agent>(e);
        auto const &transform = c.get_component<Component_Transform>(e);
        // auto const &dynamics = c.get_component<Component_Dynamics>(e);

        // Select the correct texture
        // Asset_Texture* texture;

        // Additional offsets needed since texture sizes different between animation frames
        float agent_scale = 1.0f;
        Vector2 agent_offset{ -0.5f, -0.5f };

        gr.render_texture(&agent_texture, (Vector2){ (transform.position.x + agent_offset.x) * unit_to_pixels, (transform.position.y + agent_offset.y) * unit_to_pixels }, unit_to_pixels / agent_texture.width * agent_scale, 1.0f, agent.face_forward);
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
        if (dead_index != -1 && particles.spawn_timer >= particles.spawn_time) {
            particles.spawn_timer = std::fmod(particles.spawn_timer, particles.spawn_time);

            Particle &p = particles.particles[dead_index];

            p.life = particles.lifespan;
            p.position.x = transform.position.x + particles.offset.x;
            p.position.y = transform.position.y + particles.offset.y;
        }
    }
}

void System_Particles::render() {
    const float base_alpha = 0.5f;
    const float base_scale = 0.45f;

    for (auto const &e : entities) {
        auto const &particles = c.get_component<Component_Particles>(e);

        for (int i = 0; i < particles.particles.size(); i++) {
            const Particle &p = particles.particles[i];

            if (p.life <= 0.0f)
                continue;

            float life_ratio = (particles.lifespan - p.life) / particles.lifespan;
            
            float alpha = base_alpha * (1.0f - life_ratio);
            float scale = base_scale * (0.4f * life_ratio + 0.6f);
            float offset_y = -life_ratio * 0.17f;

            gr.render_texture(&particle_texture, (Vector2){ p.position.x * unit_to_pixels - 0.5f * particle_texture.width * scale, (p.position.y + offset_y) * unit_to_pixels - 0.5f * particle_texture.height * scale }, scale * unit_to_pixels / particle_texture.width, alpha);
        }
    }
}

