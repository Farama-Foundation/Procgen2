#include "common_systems.h"

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

void System_Mob_AI::init() {
    ship_texture.load("assets/misc_assets/playerShip1_red.png");
    bullet_texture.load("assets/misc_assets/laserBlue02.png");

    explosion_textures.resize(5);

    for (int i = 0; i < explosion_textures.size(); i++)
        explosion_textures[i] = manager_texture.get("assets/misc_assets/explosion" + std::to_string(i + 1) + ".png");

    // Max bullets
    bullets.resize(64);
}

void System_Mob_AI::update(float dt) {
    // Get tile map system
    std::shared_ptr<System_Agent> agent = c.system_manager.get_system<System_Agent>();

    for (auto const &e : entities) {
        auto &mob_ai = c.get_component<Component_Mob_AI>(e);
        auto &transform = c.get_component<Component_Transform>(e);
        auto &dynamics = c.get_component<Component_Dynamics>(e);
        auto &collision = c.get_component<Component_Collision>(e);
    
        transform.position.x += dynamics.velocity.x * dt;
        transform.position.y += dynamics.velocity.y * dt;

        // World space collision
        Rectangle world_collision{ transform.position.x + collision.bounds.x, transform.position.y + collision.bounds.y, collision.bounds.width, collision.bounds.height };

    }
}

void System_Mob_AI::render() {
    assert(entities.size() == 1); // Only one player

    for (auto const &e : entities) {
        auto const &mob_ai = c.get_component<Component_Mob_AI>(e);
        auto const &transform = c.get_component<Component_Transform>(e);
        auto const &dynamics = c.get_component<Component_Dynamics>(e);

        {
            const float size = 0.1f;

            gr.render_texture(&ship_texture, { transform.position.x * unit_to_pixels - size * ship_texture.width * 0.5f, transform.position.y * unit_to_pixels - size * ship_texture.height * 0.5f }, size);
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

void System_Agent::init() {
    ship_texture.load("assets/misc_assets/playerShip1_red.png");
    bullet_texture.load("assets/misc_assets/laserBlue02.png");

    explosion_textures.resize(5);

    for (int i = 0; i < explosion_textures.size(); i++)
        explosion_textures[i] = manager_texture.get("assets/misc_assets/explosion" + std::to_string(i + 1) + ".png");

    // Max bullets
    bullets.resize(32);
}

bool System_Agent::update(float dt, const std::shared_ptr<System_Hazard> &hazard, int action) {
    bool alive = true;

    const float movement_mixrate = 0.5f;
    const float movement_speed = 0.1f;
    const float bullet_time = 2.0f;
    const float bullet_speed = 0.2f;
    const float explosion_rate = 0.5f;

    // Get tile map system
    std::shared_ptr<System_Mob_AI> mob_ai = c.system_manager.get_system<System_Mob_AI>();

    assert(entities.size() == 1); // Only one player

    // Screen rectangle
    Rectangle screen_rect{ -gr.camera_size.x / gr.camera_scale * pixels_to_unit * 0.5f, -gr.camera_size.y / gr.camera_scale * pixels_to_unit * 0.5f,
        gr.camera_size.x / gr.camera_scale * pixels_to_unit, gr.camera_size.y / gr.camera_scale * pixels_to_unit };

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

        // Move
        dynamics.velocity.x += movement_mixrate * (movement_x * movement_speed - dynamics.velocity.x) * dt;
        dynamics.velocity.y += movement_mixrate * (-movement_y * movement_speed - dynamics.velocity.y) * dt;

        transform.position.x += dynamics.velocity.x * dt;
        transform.position.y += dynamics.velocity.y * dt;

        // World space collision
        Rectangle world_collision{ transform.position.x + collision.bounds.x, transform.position.y + collision.bounds.y, collision.bounds.width, collision.bounds.height };

        // Collide with screen edges
        if (world_collision.x < screen_rect.x) {
            transform.position.x += screen_rect.x - world_collision.x;
            dynamics.velocity.x = 0.0f;
        }
        else if (world_collision.x + world_collision.width > screen_rect.x + screen_rect.width) {
            transform.position.x += screen_rect.x + screen_rect.width - (world_collision.x + world_collision.width);
            dynamics.velocity.x = 0.0f;
        }

        if (world_collision.y < screen_rect.y) {
            transform.position.y += screen_rect.y - world_collision.y;
            dynamics.velocity.y = 0.0f;
        }
        else if (world_collision.y + world_collision.height > screen_rect.y + screen_rect.height) {
            transform.position.y += screen_rect.y + screen_rect.height - (world_collision.y + world_collision.height);
            dynamics.velocity.y = 0.0f;
        }

        // Reset collision to updated collision
        world_collision = Rectangle{ transform.position.x + collision.bounds.x, transform.position.y + collision.bounds.y, collision.bounds.width, collision.bounds.height };

        if (fire) {
            if (bullet_timer == 0.0f && num_bullets < bullets.size()) {
                bullet_timer = bullet_time;

                Bullet &bullet = bullets[next_bullet];

                bullet.rotation = transform.rotation;
                bullet.vel = { 0.0f, -bullet_speed };
                bullet.pos = transform.position;
                bullet.frame = 0.0f; // First frame (bullet)

                next_bullet = (next_bullet + 1) % bullets.size();
                num_bullets++;
            }
            else
                bullet_timer = std::max(0.0f, bullet_timer - dt);
        }

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

        // Control bullets
        for (int i = 0; i < num_bullets; i++) {
            int bullet_index = (bullets.size() + next_bullet - 1 - i) % bullets.size();

            Bullet &bullet = bullets[bullet_index];

            if (bullet.frame == -1.0f)
                continue;

            if (bullet.frame == 0.0f) {
                Rectangle world_collision{ bullet.pos.x - 0.01f, bullet.pos.y - 0.01f, 0.02f, 0.02f };

                if (!check_collision(world_collision, screen_rect)) {
                    // Remove by setting to completed animation
                    bullet.vel = { 0.0f, 0.0f };
                    bullet.frame = 5.0f;
                }
                else {
                    // If collide with hazard
                    for (Entity h : hazard->get_entities()) {
                        const Component_Hazard &hazard = c.get_component<Component_Hazard>(h);
                        
                        const Component_Transform &hazard_transform = c.get_component<Component_Transform>(h);
                        const Component_Collision &hazard_collision = c.get_component<Component_Collision>(h);

                        Rectangle hazard_rect{ hazard_transform.position.x + hazard_collision.bounds.x, hazard_transform.position.y + hazard_collision.bounds.y, hazard_collision.bounds.width, hazard_collision.bounds.height };

                        if (check_collision(world_collision, hazard_rect)) {
                            // Set velocity to 0 and animate
                            bullet.vel = { 0.0f, 0.0f };
                            bullet.frame = 1.0f;

                            break;
                        }
                    }
                }
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
    }

    return alive;
}

void System_Agent::render() {
    assert(entities.size() == 1); // Only one player

    for (auto const &e : entities) {
        auto const &agent = c.get_component<Component_Agent>(e);
        auto const &transform = c.get_component<Component_Transform>(e);
        auto const &dynamics = c.get_component<Component_Dynamics>(e);

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

            const float size = 0.05f;

            gr.render_texture(texture, { bullet.pos.x * unit_to_pixels - size * texture->width * 0.5f, bullet.pos.y * unit_to_pixels - size * texture->height * 0.5f }, size);
        }

        // Render ship
        {
            const float size = 0.05f;

            gr.render_texture(&ship_texture, { transform.position.x * unit_to_pixels - size * ship_texture.width * 0.5f, transform.position.y * unit_to_pixels - size * ship_texture.height * 0.5f }, size);
        }
    }
}
