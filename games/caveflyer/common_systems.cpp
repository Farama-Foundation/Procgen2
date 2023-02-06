#include "common_systems.h"

#include "tilemap.h"

#include "helpers.h"

void System_Sprite_Render::update(float dt) {
    if (render_entities.size() != entities.size())
        render_entities.resize(entities.size());

    int index = 0;

    for (auto const &e : entities) {
        auto &sprite = c.get_component<Component_Sprite>(e);

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
    ship_texture.load("assets/misc_assets/playerShip1_red.png");
    bullet_texture.load("assets/misc_assets/laserBlue02.png");

    explosion_textures.resize(5);

    for (int i = 0; i < explosion_textures.size(); i++)
        explosion_textures[i].load("assets/misc_assets/explosion" + std::to_string(i + 1) + ".png");

    // Max bullets
    bullets.resize(32);
}

std::tuple<bool, bool, int> System_Agent::update(float dt, const std::shared_ptr<System_Hazard> &hazard, const std::shared_ptr<System_Goal> &goal, int action) {
    bool alive = true;
    bool achieved_goal = false;
    int targets_destroyed = 0;

    const float accel = 0.05f;
    const float spin_rate = 0.05f;
    const float vel_decay = 0.1f;
    const float reverse_mul = 0.5f;
    const float bullet_time = 0.5f;
    const float bullet_speed = 1.0f;
    const float explosion_rate = 0.5f;

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
        float movement_y =  (agent.action == 2 || agent.action == 5 || agent.action == 8) - (agent.action == 0 || agent.action == 3 || agent.action == 6);
        bool fire = agent.action == 9;

        if (movement_y < 0.0f)
            movement_y *= reverse_mul;

        transform.rotation += movement_x * spin_rate * dt;

        Vector2 dir{ std::cos(transform.rotation), std::sin(transform.rotation) };

        // If fire
        if (fire) {
            if (bullet_timer == 0.0f && num_bullets < bullets.size()) {
                bullet_timer = bullet_time;

                Bullet &bullet = bullets[next_bullet];

                bullet.rotation = transform.rotation;
                bullet.vel = { dir.x * bullet_speed, dir.y * bullet_speed };
                bullet.pos = transform.position;
                bullet.frame = 0.0f; // First frame (bullet)

                next_bullet = (next_bullet + 1) % bullets.size();
                num_bullets++;
            }
            else
                bullet_timer = std::max(0.0f, bullet_timer - dt);
        }

        Vector2 acceleration{ dir.x * movement_y * accel, dir.y * movement_y * accel };
        
        dynamics.velocity.x += (acceleration.x - dynamics.velocity.x * vel_decay) * dt;
        dynamics.velocity.y += (acceleration.y - dynamics.velocity.y * vel_decay) * dt;

        // Move
        transform.position.x += dynamics.velocity.x * dt;
        transform.position.y += dynamics.velocity.y * dt;

        // World space collision
        Rectangle world_collision{ transform.position.x + collision.bounds.x, transform.position.y + collision.bounds.y, collision.bounds.width, collision.bounds.height };

        std::pair<Vector2, bool> collision_data = tilemap->get_collision(world_collision, [](Tile_ID id) -> Collision_Type {
            return (id == wall ? full : none);
        });

        // If was moved up, on ground
        Vector2 delta_position{ collision_data.first.x - world_collision.x, collision_data.first.y - world_collision.y };

        // Correct position
        transform.position.x = collision_data.first.x - collision.bounds.x;
        transform.position.y = collision_data.first.y - collision.bounds.y;

        // Update world collision
        world_collision.x = transform.position.x + collision.bounds.x;
        world_collision.y = transform.position.y + collision.bounds.y;

        if (delta_position.x != 0.0f)
            dynamics.velocity.x = 0.0f;

        if (delta_position.y != 0.0f)
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
        gr.camera_position.y = transform.position.y * unit_to_pixels;

        // Control bullets
        for (int i = 0; i < num_bullets; i++) {
            int bullet_index = (bullets.size() + next_bullet - 1 - i) % bullets.size();

            Bullet &bullet = bullets[bullet_index];

            if (bullet.frame == -1.0f)
                continue;

            if (bullet.frame == 0.0f) {
                Rectangle world_collision{ bullet.pos.x - 0.01f, bullet.pos.y - 0.01f, 0.02f, 0.02f };

                std::pair<Vector2, bool> collision_data = tilemap->get_collision(world_collision, [](Tile_ID id) -> Collision_Type {
                    return (id == wall ? full : none);
                });

                if (collision_data.second) {
                    // Set velocity to 0 and animate
                    bullet.vel = { 0.0f, 0.0f };
                    bullet.frame = 1.0f;
                }

                std::vector<Entity> to_destroy;

                // If collide with hazard
                for (Entity h : hazard->get_entities()) {
                    const Component_Hazard &hazard = c.get_component<Component_Hazard>(h);
                    
                    if (!hazard.destroyable)
                        continue;

                    const Component_Transform &hazard_transform = c.get_component<Component_Transform>(h);
                    const Component_Collision &hazard_collision = c.get_component<Component_Collision>(h);

                    Rectangle hazard_rect{ hazard_transform.position.x + hazard_collision.bounds.x, hazard_transform.position.y + hazard_collision.bounds.y, hazard_collision.bounds.width, hazard_collision.bounds.height };

                    if (check_collision(world_collision, hazard_rect)) {
                        // Set velocity to 0 and animate
                        bullet.vel = { 0.0f, 0.0f };
                        bullet.frame = 1.0f;

                        // Destroy the hazard
                        to_destroy.push_back(h);

                        targets_destroyed++;

                        break;
                    }
                }

                for (int i = 0; i < to_destroy.size(); i++)
                    c.destroy_entity(to_destroy[i]);
            }

            // Move
            bullet.pos.x += bullet.vel.x * dt;
            bullet.pos.y += bullet.vel.y * dt;

            if (bullet.frame >= 5.0f) {
                // Destroy
                num_bullets--;
                bullet.frame = -1.0f;
            }
            else if (bullet.frame >= 1.0f)
                bullet.frame += explosion_rate * dt;
        }
            
        // Enable particles on jump
        auto &particles = c.get_component<Component_Particles>(e);

        particles.enabled = movement_y > 0.0f;
    }

    return std::make_tuple(alive, achieved_goal, targets_destroyed);
}

void System_Agent::render() {
    assert(entities.size() == 1); // Only one player

    for (auto const &e : entities) {
        auto const &agent = c.get_component<Component_Agent>(e);
        auto const &transform = c.get_component<Component_Transform>(e);
        auto const &dynamics = c.get_component<Component_Dynamics>(e);

        {
            const float size = 0.15f;

            gr.render_texture_rotated(&ship_texture, { transform.position.x * unit_to_pixels - size * ship_texture.width * 0.5f, transform.position.y * unit_to_pixels - size * ship_texture.height * 0.5f }, transform.rotation + M_PI * 0.5f, size);
        }

        // Render bullets
        for (int i = 0; i < num_bullets; i++) {
            int bullet_index = (bullets.size() + next_bullet - 1 - i) % bullets.size();

            Bullet &bullet = bullets[bullet_index];

            if (bullet.frame == -1.0f)
                continue;

            Asset_Texture* texture;

            if (bullet.frame == 0.0f)
                texture = &bullet_texture;
            else
                texture = &explosion_textures[static_cast<int>(bullet.frame - 1.0f)];

            const float size = 0.1f;

            gr.render_texture_rotated(texture, { bullet.pos.x * unit_to_pixels - size * texture->width * 0.5f, bullet.pos.y * unit_to_pixels - size * texture->height * 0.5f }, bullet.rotation + M_PI * 0.5f, size);
        }
    }
}

void System_Particles::init() {
    particle_texture.load("assets/misc_assets/towerDefense_tile295.png");
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
        if (dead_index != -1 && particles.spawn_timer >= particles.spawn_time && particles.enabled) {
            particles.spawn_timer = std::fmod(particles.spawn_timer, particles.spawn_time);

            Particle &p = particles.particles[dead_index];

            p.life = particles.lifespan;

            p.rotation = transform.rotation + M_PI * 0.5f;

            float c = std::cos(p.rotation);
            float s = std::sin(p.rotation);

            p.dir = { -std::cos(transform.rotation), -std::sin(transform.rotation) };

            Vector2 rotated_offset{ c * particles.offset.x - s * particles.offset.y, s * particles.offset.x + c * particles.offset.y };

            p.position.x = transform.position.x + rotated_offset.x;
            p.position.y = transform.position.y + rotated_offset.y;
        }
    }
}

void System_Particles::render() {
    const float base_alpha = 0.5f;
    const float base_scale = 1.0f;

    for (auto const &e : entities) {
        auto const &particles = c.get_component<Component_Particles>(e);

        for (int i = 0; i < particles.particles.size(); i++) {
            const Particle &p = particles.particles[i];

            if (p.life <= 0.0f)
                continue;

            float life_ratio = (particles.lifespan - p.life) / particles.lifespan;
            
            float alpha = base_alpha * (1.0f - life_ratio);
            float scale = base_scale * (0.4f * life_ratio + 0.6f);
            float shift = life_ratio * 2.0f;
            float size = scale * unit_to_pixels / particle_texture.width;

            gr.render_texture_rotated(&particle_texture, (Vector2){ (p.position.x + p.dir.x * shift) * unit_to_pixels - size * particle_texture.width * 0.5f, (p.position.y + p.dir.y * shift) * unit_to_pixels - size * particle_texture.height * 0.5f }, p.rotation, size, alpha);
        }
    }
}

