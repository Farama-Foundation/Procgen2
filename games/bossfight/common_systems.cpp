#include "common_systems.h"

#include "helpers.h"
#include <iostream>

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
        gr.render_texture(sprite.texture, (Vector2){ (transform.position.x + sprite.position.x) * unit_to_pixels, (transform.position.y + sprite.position.y) * unit_to_pixels }, scale * unit_to_pixels / sprite.texture->width, 1.0f, sprite.flip_x, sprite.flip_y);
    }
}

void System_Mob_AI::init() {
    ship_textures.resize(4);

    ship_textures[0].load("assets/misc_assets/enemyShipBlack1.png");
    ship_textures[1].load("assets/misc_assets/enemyShipBlue2.png");
    ship_textures[2].load("assets/misc_assets/enemyShipGreen3.png");
    ship_textures[3].load("assets/misc_assets/enemyShipRed4.png");

    bullet_textures.resize(3);

    bullet_textures[0] = &manager_texture.get("assets/misc_assets/laserGreen14.png");
    bullet_textures[1] = &manager_texture.get("assets/misc_assets/laserRed11.png");
    bullet_textures[2] = &manager_texture.get("assets/misc_assets/laserBlue09.png");

    explosion_textures.resize(5);

    for (int i = 0; i < explosion_textures.size(); i++)
        explosion_textures[i] = &manager_texture.get("assets/misc_assets/explosion" + std::to_string(i + 1) + ".png");

    shield_texture.load("assets/misc_assets/shield2.png");

    // Max bullets and explosions
    bullets.resize(64);
    explosions.resize(8);
}

void System_Mob_AI::fire(const Vector2 &pos, float rotation, float speed) {
    if (num_bullets < bullets.size()) {
        Bullet &bullet = bullets[next_bullet];

        bullet.rotation = rotation;
        bullet.vel = { std::cos(rotation) * speed, -std::sin(rotation) * speed };
        bullet.pos = pos;
        bullet.frame = 0.0f; // First frame (bullet)

        next_bullet = (next_bullet + 1) % bullets.size();
        num_bullets++;
    }
}

// Spawn en explosion
void System_Mob_AI::explode(const Vector2 &pos) {
    if (num_explosions < explosions.size()) {
        Explosion &explosion = explosions[next_explosion];

        explosion.pos = pos;
        explosion.frame = 0.0f; // First frame

        next_explosion = (next_explosion + 1) % explosions.size();
        num_explosions++;
    }
}

// Different attack patterns
void System_Mob_AI::fire_pattern(const Vector2 &pos, int pattern_index, float &timer, float dt, std::mt19937 &rng) {
    const float bullet_speed = config.mode == hard_mode ? 0.1f : 0.05f;

    switch (pattern_index) {
    case -1: // Passive
    {
        std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

        if (dist01(rng) < 0.1f * dt)
            fire(pos, M_PI * (1.0f + dist01(rng)), bullet_speed);

        break;
    }
    case 0:
    {
        if (timer >= 8.0f) {
            timer = 0.0f;

            for (int i = 0; i < 5; i++) {
                float rotation = M_PI * 1.5f + (i - 2) * M_PI * 0.125f;

                fire(pos, rotation, bullet_speed);
            }
        }
        else
            timer += dt;

        break;
    }
    case 1:
    {
        if (timer >= 5.0f) {
            timer = 0.0f;

            int k = timer / 5.0f;
            
            k = abs(8 - (k % 16));

            for (int i = 0; i < 4; i++) {
                float rotation = M_PI * (1.25f + k * 0.0625f) + i * M_PI * 0.5f;

                fire(pos, rotation, bullet_speed);
            }
        }
        else
            timer += dt;

        break;
    }
    case 2:
    {
        if (timer >= 10.0f) {
            timer = 0.0f;

            std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

            float offset = dist01(rng) * 2.0f * M_PI;

            for (int i = 0; i < 8; i++) {
                float rotation = M_PI * 0.25f * i + offset;

                fire(pos, rotation, bullet_speed);
            }
        }
        else
            timer += dt;

        break;
    }
    case 3:
        if (timer >= 4.0f) {
            timer = 0.0f;

            std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

            fire(pos, M_PI * (1.0f + dist01(rng)), bullet_speed);
        }
        else
            timer += dt;

        break;
    }
}

void System_Mob_AI::show_damage(const Vector2 &pos, float dt, std::mt19937 &rng) {
    if (explosion_timer >= 8.0f) {
        explosion_timer = 0.0f;

        std::uniform_real_distribution<float> damage_dist(-0.5f, 0.5f);

        explode(Vector2{ damage_dist(rng) + pos.x, damage_dist(rng) + pos.y });
    }
    else
        explosion_timer += dt;
}

bool System_Mob_AI::update(float dt, const std::shared_ptr<System_Hazard> &hazard, std::mt19937 &rng) {
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    const float shielded_phase_time = 180.0f + dist01(rng) * (config.mode == hard_mode ? 80.0f : 30.0f); // Time to stay in a shielded phase
    const float unshielded_phase_time = 300.0f; // Next phase triggered by hit by player mostly
    const float attack_time = 8.0f;
    const int num_weapons = 4;
    const float explosion_rate = 0.3f;
    const float move_time = 70.0f;
    const int boss_hp = 3;
    const float damage_time = 80.0f;

    bool alive = true;

    // Get agent system
    std::shared_ptr<System_Agent> agent = c.system_manager.get_system<System_Agent>();

    // Gather some information about the agent
    Entity e_agent = *agent->entities.begin();

    const Component_Transform &agent_transform = c.get_component<Component_Transform>(e_agent);
    const Component_Collision &agent_collision = c.get_component<Component_Collision>(e_agent);

    Rectangle agent_rect{ agent_transform.position.x + agent_collision.bounds.x, agent_transform.position.y + agent_collision.bounds.y, agent_collision.bounds.width, agent_collision.bounds.height };

    assert(entities.size() == 1); // Only 1 boss

    // Screen rectangle
    Rectangle screen_rect{ -gr.camera_size.x / gr.camera_scale * pixels_to_unit * 0.5f, -gr.camera_size.y / gr.camera_scale * pixels_to_unit * 0.5f,
        gr.camera_size.x / gr.camera_scale * pixels_to_unit, gr.camera_size.y / gr.camera_scale * pixels_to_unit };

    // For all entities (only 1, the boss)
    for (auto const &e : entities) {
        auto &mob_ai = c.get_component<Component_Mob_AI>(e);
        auto &transform = c.get_component<Component_Transform>(e);
        auto &dynamics = c.get_component<Component_Dynamics>(e);
        auto &collision = c.get_component<Component_Collision>(e);

        if (mob_ai.phase_timer == 0.0f) { // Phase start, set some values
            std::uniform_int_distribution<int> weapon_dist(0, num_weapons - 1);

            mob_ai.weapon_index = weapon_dist(rng);
            mob_ai.attack_timer = 0.0f;
            mob_ai.hp = boss_hp;
        }

        if (mob_ai.phase_index % 2 == 0) { // Shielded phase
            if (mob_ai.phase_timer >= shielded_phase_time) {
                // Next phase
                mob_ai.phase_timer = 0.0f;

                mob_ai.phase_index++;
            }
            else
                mob_ai.phase_timer += dt;

            // Spawn bullets
            fire_pattern(transform.position, mob_ai.weapon_index, mob_ai.attack_timer, dt, rng);
        }
        else { // Unshielded phase
            if (mob_ai.phase_timer >= unshielded_phase_time) {
                // Next phase
                mob_ai.phase_timer = 0.0f;

                mob_ai.phase_index++;
            }
            else
                mob_ai.phase_timer += dt;

            fire_pattern(transform.position, -1, mob_ai.attack_timer, dt, rng); // Passive attack (index -1)

            // If HP depleted, show some explosions
            if (mob_ai.hp == 0) {
                show_damage(transform.position, dt, rng);
                
                if (damage_timer >= damage_time) { // If done showing damage, go to next phase
                    damage_timer = 0.0f;

                    mob_ai.phase_index++;
                    mob_ai.hp = boss_hp; // Reset HP
                }
                else
                    damage_timer += dt;
            }
        }

        // Movement
        if (move_timer >= move_time) {
            move_timer = 0.0f;

            Vector2 target_pos{ (dist01(rng) * 2.0f - 1.0f) * 0.5f * screen_rect.width * 0.7f, ((dist01(rng) * 2.0f - 1.0f) * 0.5f - 0.3f) * screen_rect.height * 0.5f };

            dynamics.velocity.x = (target_pos.x - transform.position.x) / move_time;
            dynamics.velocity.y = (target_pos.y - transform.position.y) / move_time;
        }
        else
            move_timer += dt;

        transform.position.x += dynamics.velocity.x * dt;
        transform.position.y += dynamics.velocity.y * dt;

        // World space collision
        Rectangle world_collision{ transform.position.x + collision.bounds.x, transform.position.y + collision.bounds.y, collision.bounds.width, collision.bounds.height };

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
                    // If collide with agent
                    if (check_collision(world_collision, agent_rect)) {
                        // Set velocity to 0 and animate
                        bullet.vel = { 0.0f, 0.0f };
                        bullet.frame = 1.0f;

                        agent->alive = false;

                        break;
                    }

                    for (Entity h : hazard->get_entities()) {
                        if (h == e) // Skip self (also a hazard)
                            continue;

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

        // Control explosions
        for (int i = 0; i < num_explosions; i++) {
            int explosion_index = (explosions.size() + next_explosion - 1 - i) % explosions.size();

            Explosion &explosion = explosions[explosion_index];

            if (explosion.frame == -1.0f)
                continue;

            if (explosion.frame >= 4.0f) {
                // Destroy
                num_explosions--;
                explosion.frame = -1.0f;
            }
            else if (explosion.frame >= 0.0f)
                explosion.frame += explosion_rate * dt;
        }

        if (mob_ai.phase_index >= 6) // 6 since there are 2 sub-phases per damage phase (3)
            alive = false;
    }

    return alive;
}

void System_Mob_AI::render() {
    assert(entities.size() == 1); // Only one player

    for (auto const &e : entities) {
        auto const &mob_ai = c.get_component<Component_Mob_AI>(e);
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
                texture = bullet_textures[current_bullet_texture_index];
            else
                texture = explosion_textures[static_cast<int>(bullet.frame - 1.0f)];

            const float size = 0.1f;

            gr.render_texture_rotated(texture, { bullet.pos.x * unit_to_pixels - size * texture->width * 0.5f, bullet.pos.y * unit_to_pixels - size * texture->height * 0.5f }, bullet.rotation + M_PI * 0.5f, size);
        }

        {
            const float size = 0.25f;

            gr.render_texture(&ship_textures[current_ship_texture_index], { transform.position.x * unit_to_pixels - size * ship_textures[current_ship_texture_index].width * 0.5f, transform.position.y * unit_to_pixels - size * ship_textures[current_ship_texture_index].height * 0.5f }, size);
        }

        // Render shield if in a shield phase
        if (mob_ai.phase_index % 2 == 0) {
            const float size = 0.25f;

            gr.render_texture(&shield_texture, { transform.position.x * unit_to_pixels - size * shield_texture.width * 0.5f, transform.position.y * unit_to_pixels - size * shield_texture.height * 0.5f }, size, 0.7f); // Some transparency
        }

        // Render explosions
        for (int i = 0; i < num_explosions; i++) {
            int explosion_index = (explosions.size() + next_explosion - 1 - i) % explosions.size();

            Explosion &explosion = explosions[explosion_index];

            if (explosion.frame == -1.0f)
                continue;

            Asset_Texture* texture = explosion_textures[static_cast<int>(explosion.frame)];

            const float size = 0.3f;

            gr.render_texture(texture, { explosion.pos.x * unit_to_pixels - size * texture->width * 0.5f, explosion.pos.y * unit_to_pixels - size * texture->height * 0.5f }, size);
        }
    }
}

void System_Mob_AI::reset(std::mt19937 &rng) {
    next_bullet = 0;
    next_explosion = 0;
    num_bullets = 0;
    num_explosions = 0;
    bullet_timer = 0.0f;
    explosion_timer = 0.0f;
    damage_timer = 0.0f;
    move_timer = 0.0f;

    std::uniform_int_distribution<int> ship_texture_dist(0, ship_textures.size() - 1);

    current_ship_texture_index = ship_texture_dist(rng);

    std::uniform_int_distribution<int> bullet_texture_dist(0, bullet_textures.size() - 1);

    current_bullet_texture_index = bullet_texture_dist(rng);
}

void System_Agent::init() {
    ship_textures.resize(4);

    ship_textures[0].load("assets/misc_assets/playerShip1_blue.png");
    ship_textures[1].load("assets/misc_assets/playerShip1_green.png");
    ship_textures[2].load("assets/misc_assets/playerShip2_orange.png");
    ship_textures[3].load("assets/misc_assets/playerShip3_red.png");

    bullet_textures.resize(3);

    bullet_textures[0] = &manager_texture.get("assets/misc_assets/laserGreen14.png");
    bullet_textures[1] = &manager_texture.get("assets/misc_assets/laserRed11.png");
    bullet_textures[2] = &manager_texture.get("assets/misc_assets/laserBlue09.png");

    explosion_textures.resize(5);

    for (int i = 0; i < explosion_textures.size(); i++)
        explosion_textures[i] = &manager_texture.get("assets/misc_assets/explosion" + std::to_string(i + 1) + ".png");

    // Max bullets
    bullets.resize(32);
}

bool System_Agent::update(float dt, const std::shared_ptr<System_Hazard> &hazard, int action, std::mt19937 &rng) {
    const float movement_mixrate = 0.5f;
    const float movement_speed = 0.1f;
    const float bullet_time = 5.0f;
    const float bullet_speed = 0.1f;
    const float bullet_bounce_speed = 0.05f;
    const float bounce_time = 10.0f;
    const float explosion_rate = 0.3f;

    // Get tile map system
    std::shared_ptr<System_Mob_AI> mob_ai = c.system_manager.get_system<System_Mob_AI>();

    // First and only entity in mob_ai (the boss)
    Entity boss = *mob_ai->entities.begin();
    auto &boss_mob_ai = c.get_component<Component_Mob_AI>(boss);

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
                bullet.bouncing = false;
                bullet.bounce_timer = 0.0f;

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
                            if (h == boss) {
                                if (boss_mob_ai.phase_index % 2 == 0) { // Boss and is in a shield phase
                                    // Set velocity to 0 and bounce
                                    std::uniform_real_distribution<float> bounce_dist(-1.0f, 1.0f);

                                    bullet.vel = { bounce_dist(rng) * bullet_bounce_speed, bullet_bounce_speed };

                                    bullet.bounce_timer = bounce_time;
                                    bullet.bouncing = true;
                                }
                                else {
                                    // Set velocity to 0 and animate
                                    bullet.vel = { 0.0f, 0.0f };
                                    bullet.frame = 1.0f;

                                    if (boss_mob_ai.hp > 0)
                                        boss_mob_ai.hp--;
                                }
                            }
                            else {
                                // Set velocity to 0 and animate
                                bullet.vel = { 0.0f, 0.0f };
                                bullet.frame = 1.0f;
                            }

                            break;
                        }
                    }
                }
            }

            // Move
            bullet.pos.x += bullet.vel.x * dt;
            bullet.pos.y += bullet.vel.y * dt;

            bool destroy_bullet = false;

            if (bullet.frame >= 5.0f)
                destroy_bullet = true;
            else if (bullet.frame >= 1.0f)
                bullet.frame += explosion_rate * dt;

            if (bullet.bouncing) {
                if (bullet.bounce_timer > 0.0f)
                    bullet.bounce_timer = std::max(0.0f, bullet.bounce_timer - dt);
                else
                    destroy_bullet = true;
            }

            if (destroy_bullet) {
                num_bullets--;
                bullet.frame = -1.0f;
            }
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
                texture = bullet_textures[current_bullet_texture_index];
            else
                texture = explosion_textures[static_cast<int>(bullet.frame - 1.0f)];

            const float size = 0.05f;

            gr.render_texture(texture, { bullet.pos.x * unit_to_pixels - size * texture->width * 0.5f, bullet.pos.y * unit_to_pixels - size * texture->height * 0.5f }, size);
        }

        // Render ship
        {
            const float size = 0.05f;

            gr.render_texture(&ship_textures[current_ship_texture_index], { transform.position.x * unit_to_pixels - size * ship_textures[current_ship_texture_index].width * 0.5f, transform.position.y * unit_to_pixels - size * ship_textures[current_ship_texture_index].height * 0.5f }, size);
        }
    }
}

void System_Agent::reset(std::mt19937 &rng) {
    next_bullet = 0;
    num_bullets = 0;
    bullet_timer = 0.0f;

    std::uniform_int_distribution<int> ship_texture_dist(0, ship_textures.size() - 1);

    current_ship_texture_index = ship_texture_dist(rng);

    std::uniform_int_distribution<int> bullet_texture_dist(0, bullet_textures.size() - 1);

    current_bullet_texture_index = bullet_texture_dist(rng);

    alive = true;
}
