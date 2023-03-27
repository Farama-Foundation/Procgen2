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
    std::shared_ptr<System_Agent> agent = c.system_manager.get_system<System_Agent>();
    const auto &agent_transform = c.get_component<Component_Transform>(agent->get_info().entity);
    const auto &agent_collision = c.get_component<Component_Collision>(agent->get_info().entity);

    std::shared_ptr<System_Mob_AI> mob_ai = c.system_manager.get_system<System_Mob_AI>();

    Rectangle agent_rect = agent_collision.bounds;
    agent_rect.x += agent_transform.position.x;
    agent_rect.y += agent_transform.position.y;

    num_points_available = 0;
    point_delta = 0;

    std::vector<Entity> to_destroy;

    for (auto const &e : entities) {
        auto &transform = c.get_component<Component_Transform>(e);
        auto &collision = c.get_component<Component_Collision>(e);
        auto &point = c.get_component<Component_Point>(e);

        Rectangle rect = collision.bounds;
        rect.x += transform.position.x;
        rect.y += transform.position.y;

        if (check_collision(agent_rect, rect)) {
            if (point.is_orb)
                mob_ai->eat();
                
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

void System_Mob_AI::init() {
    anim_textures.resize(4);

    anim_textures[0].load("assets/misc_assets/enemyFlying_1.png");
    anim_textures[1].load("assets/misc_assets/enemyFlying_2.png");
    anim_textures[2].load("assets/misc_assets/enemyFlying_3.png");
    anim_textures[3].load("assets/misc_assets/enemyWalking_1b.png");
}

bool System_Mob_AI::update(float dt, std::mt19937 &rng) {
    const float hatch_time = 50.0f;
    const float anim_time = 1.0f;

    const float speed_low = 0.125f;
    const float speed_high = 0.25f;

    bool player_hit = false;

    // Hatched only
    std::shared_ptr<System_Tilemap> tilemap = c.system_manager.get_system<System_Tilemap>();
    std::shared_ptr<System_Agent> agent = c.system_manager.get_system<System_Agent>();

    const auto &agent_transform = c.get_component<Component_Transform>(agent->get_info().entity);
    const auto &agent_collision = c.get_component<Component_Collision>(agent->get_info().entity);

    Rectangle agent_rect = agent_collision.bounds;
    agent_rect.x += agent_transform.position.x;
    agent_rect.y += agent_transform.position.y;

    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    for (auto const &e : entities) {
        auto &mob_ai = c.get_component<Component_Mob_AI>(e);

        if (mob_ai.hatch_timer >= hatch_time) {
            auto &transform = c.get_component<Component_Transform>(e);
            auto &sprite = c.get_component<Component_Sprite>(e);
            auto &collision = c.get_component<Component_Collision>(e);
            auto &dynamics = c.get_component<Component_Dynamics>(e);

            float speed;

            if (eat_timer == 0.0f) { // Eat timer is depleted
                // Choose texture - flap down
                if (anim_index < 3)
                    sprite.texture = &anim_textures[anim_index];
                else // Flap up
                    sprite.texture = &anim_textures[5 - anim_index];

                speed = speed_high;
            }
            else { // Running away
                sprite.texture = &anim_textures[3];

                speed = speed_low;
            }

            bool at_junction = std::max(abs(transform.position.x - (static_cast<int>(transform.position.x) + 0.5f)),
                abs(transform.position.y - (static_cast<int>(transform.position.y) + 0.5f))) < speed * dt;

            // If not moving or at a junction
            if ((dynamics.velocity.x == 0.0f && dynamics.velocity.y == 0.0f) || at_junction) {
                int dir_index = 0;
                int num_possibilities = 0;

                // Scan possible moves 
                for (int dx = -1; dx <= 1; dx += 2) {
                    Tile_ID id = tilemap->get(static_cast<int>(transform.position.x) + dx, tilemap->get_height() - 1 - static_cast<int>(transform.position.y)); 

                    dir_possibilities[dir_index] = (id == empty && dx != -sign(dynamics.velocity.x));

                    if (dir_possibilities[dir_index])
                        num_possibilities++;

                    dir_index++;
                }

                for (int dy = -1; dy <= 1; dy += 2) {
                    Tile_ID id = tilemap->get(static_cast<int>(transform.position.x), tilemap->get_height() - 1 - (static_cast<int>(transform.position.y) + dy)); 

                    dir_possibilities[dir_index] = (id == empty && dy != -sign(dynamics.velocity.y));

                    if (dir_possibilities[dir_index])
                        num_possibilities++;

                    dir_index++;
                }

                bool be_aggressive = dist01(rng) < 0.5f;

                int select_index = 0;

                if (be_aggressive) {
                    float min_dist = 999999.0f;

                    // Choose direction of agent if possible
                    for (int i = 0; i < dir_possibilities.size(); i++) {
                        if (dir_possibilities[i]) {
                            float manhattan_dist = abs(transform.position.x + directions[i].x - agent_transform.position.x) + abs(transform.position.y + directions[i].y - agent_transform.position.y);

                            // Run away when eat timer is active
                            if (eat_timer > 0.0f)
                                manhattan_dist = -manhattan_dist;

                            if (manhattan_dist < min_dist) {
                                min_dist = manhattan_dist;
                                select_index = i;
                            }
                        }
                    }
                }
                else {
                    if (num_possibilities > 0) {
                        // Roulette wheel selection of a possibility
                        std::uniform_int_distribution<int> cusp_dist(0, num_possibilities - 1);

                        int rand_cusp = cusp_dist(rng);

                        int sum_so_far = 0;

                        for (int i = 0; i < dir_possibilities.size(); i++) {
                            sum_so_far += dir_possibilities[i];

                            if (sum_so_far > rand_cusp) {
                                select_index = i;
                                break;
                            }
                        }
                    }
                }

                // Change direction
                dynamics.velocity.x = directions[select_index].x * speed;
                dynamics.velocity.y = directions[select_index].y * speed;

                // Stay aligned
                if (directions[select_index].x == 0.0f)
                    transform.position.x = static_cast<int>(transform.position.x) + 0.5f;

                if (directions[select_index].y == 0.0f)
                    transform.position.y = static_cast<int>(transform.position.y) + 0.5f;
            }

            transform.position.x += dynamics.velocity.x * dt;
            transform.position.y += dynamics.velocity.y * dt;

            Rectangle rect = collision.bounds;
            rect.x += transform.position.x;
            rect.y += transform.position.y;

            if (check_collision(agent_rect, rect)) {
                if (eat_timer == 0.0f)
                    // Player loses
                    player_hit = true;
                else {
                    // Respawn as an egg
                    mob_ai.hatch_timer = 0.0f;

                    // Set to egg sprite again
                    Asset_Texture* texture = &manager_texture.get("assets/misc_assets/enemySpikey_1b.png");

                    std::uniform_int_distribution<int> free_cell_dist(0, tilemap->free_cells.size() - 1);

                    int free_cell_index = tilemap->free_cells[free_cell_dist(rng)];

                    transform.position.x = free_cell_index / tilemap->get_height() + 0.5f;
                    transform.position.y = free_cell_index % tilemap->get_height() + 0.5f;

                    sprite.texture = texture;
                }
            }
        }
        else
            mob_ai.hatch_timer += dt;
    }

    if (anim_timer < anim_time)
        anim_timer += dt;
    else {
        anim_timer -= anim_time; // Reset with overflow
        anim_index = (anim_index + 1) % 6;
    }

    if (eat_timer > 0.0f)
        eat_timer = std::max(0.0f, eat_timer - dt);

    return player_hit;
}

void System_Mob_AI::eat() {
    eat_timer = 75.0f;
}

void System_Agent::init() {
    agent_texture.load("assets/misc_assets/enemyFloating_1b.png");
}

bool System_Agent::update(float dt, int action) {
    bool alive = true;

    // Parameters
    const float speed = 0.2f;
    const float input_reset_time = 1.0f / speed * 0.5f;

    // Get tile map system
    std::shared_ptr<System_Tilemap> tilemap = c.system_manager.get_system<System_Tilemap>();
    std::shared_ptr<System_Point> point = c.system_manager.get_system<System_Point>();

    assert(entities.size() == 1); // Only one player

    for (auto const &e : entities) {
        auto &agent = c.get_component<Component_Agent>(e);

        // Set action
        agent.action = action;

        auto &transform = c.get_component<Component_Transform>(e);
        auto &dynamics = c.get_component<Component_Dynamics>(e);

        const auto &collision = c.get_component<Component_Collision>(e);

        float movement_x = (agent.action == 7) - (agent.action == 1);
        float movement_y = (agent.action == 3) - (agent.action == 5);

        // Can't move diagonally
        if (movement_x != 0.0f && movement_y != 0.0f)
            movement_y = 0.0f;

        Vector2 desired_velocity = Vector2{ movement_x, movement_y };

        // If non-zero desired velocity, set next to it
        if (desired_velocity.x != 0.0f || desired_velocity.y != 0.0f) {
            agent.next_velocity = desired_velocity;
            input_timer = 0.0f;
        }

        // If can change velocity to the next velocity (within some error), do so
        if (agent.next_velocity.x > 0.0f) {
            if (abs(transform.position.y - (static_cast<int>(transform.position.y) + 0.5f)) <= speed * dt) {
                Tile_ID id = tilemap->get(static_cast<int>(transform.position.x) + 1, tilemap->get_height() - 1 - static_cast<int>(transform.position.y)); 

                if (id == empty) {
                    transform.position.y = static_cast<int>(transform.position.y) + 0.5f;
                    dynamics.velocity = agent.next_velocity;
                }
            }
        }
        else if (agent.next_velocity.x < 0.0f) {
            if (abs(transform.position.y - (static_cast<int>(transform.position.y) + 0.5f)) <= speed * dt) {
                Tile_ID id = tilemap->get(static_cast<int>(transform.position.x) - 1, tilemap->get_height() - 1 - static_cast<int>(transform.position.y)); 

                if (id == empty) {
                    transform.position.y = static_cast<int>(transform.position.y) + 0.5f;
                    dynamics.velocity = agent.next_velocity;
                }
            }
        }

        if (agent.next_velocity.y > 0.0f) {
            if (abs(transform.position.x - (static_cast<int>(transform.position.x) + 0.5f)) <= speed * dt) {
                Tile_ID id = tilemap->get(static_cast<int>(transform.position.x), tilemap->get_height() - 1 - (static_cast<int>(transform.position.y) + 1)); 

                if (id == empty) {
                    transform.position.x = static_cast<int>(transform.position.x) + 0.5f;
                    dynamics.velocity = agent.next_velocity;
                }
            }
        }
        else if (agent.next_velocity.y < 0.0f) {
            if (abs(transform.position.x - (static_cast<int>(transform.position.x) + 0.5f)) <= speed * dt) {
                Tile_ID id = tilemap->get(static_cast<int>(transform.position.x), tilemap->get_height() - 1 - (static_cast<int>(transform.position.y) - 1)); 

                if (id == empty) {
                    transform.position.x = static_cast<int>(transform.position.x) + 0.5f;
                    dynamics.velocity = agent.next_velocity;
                }
            }
        }

        // Collisions
        if (dynamics.velocity.x < 0.0f) {
            if (abs(transform.position.x - (static_cast<int>(transform.position.x) + 0.5f)) <= speed * dt) {
                Tile_ID id = tilemap->get(static_cast<int>(transform.position.x) - 1, tilemap->get_height() - 1 - static_cast<int>(transform.position.y)); 

                if (id != empty) {
                    transform.position.x = static_cast<int>(transform.position.x) + 0.5f;
                    dynamics.velocity.x = 0.0f;
                }
            }
        }
        else if (dynamics.velocity.x > 0.0f) {
            if (abs(transform.position.x - (static_cast<int>(transform.position.x) + 0.5f)) <= speed * dt) {
                Tile_ID id = tilemap->get(static_cast<int>(transform.position.x) + 1, tilemap->get_height() - 1 - static_cast<int>(transform.position.y)); 

                if (id != empty) {
                    transform.position.x = static_cast<int>(transform.position.x) + 0.5f;
                    dynamics.velocity.x = 0.0f;
                }
            }
        }

        if (dynamics.velocity.y < 0.0f) {
            if (abs(transform.position.y - (static_cast<int>(transform.position.y) + 0.5f)) <= speed * dt) {
                Tile_ID id = tilemap->get(static_cast<int>(transform.position.x), tilemap->get_height() - 1 - (static_cast<int>(transform.position.y) - 1)); 

                if (id != empty) {
                    transform.position.y = static_cast<int>(transform.position.y) + 0.5f;
                    dynamics.velocity.y = 0.0f;
                }
            }
        }
        else if (dynamics.velocity.y > 0.0f) {
            if (abs(transform.position.y - (static_cast<int>(transform.position.y) + 0.5f)) <= speed * dt) {
                Tile_ID id = tilemap->get(static_cast<int>(transform.position.x), tilemap->get_height() - 1 - (static_cast<int>(transform.position.y) + 1)); 

                if (id != empty) {
                    transform.position.y = static_cast<int>(transform.position.y) + 0.5f;
                    dynamics.velocity.y = 0.0f;
                }
            }
        }

        // Move
        transform.position.x += dynamics.velocity.x * speed * dt;
        transform.position.y += dynamics.velocity.y * speed * dt;

        if (input_timer >= input_reset_time)
            agent.next_velocity = { 0.0f, 0.0f };
        else
            input_timer += dt;

        // Update info
        info.entity = e;
    }

    return alive;
}

void System_Agent::render() {
    assert(entities.size() == 1); // Only one player

    for (auto const &e : entities) {
        auto const &agent = c.get_component<Component_Agent>(e);
        auto const &transform = c.get_component<Component_Transform>(e);
        auto const &dynamics = c.get_component<Component_Dynamics>(e);

        // Additional offsets needed since texture sizes different between animation frames
        float agent_scale = 1.0f;
        Vector2 agent_offset{ -0.5f, -0.5f };

        gr.render_texture(&agent_texture, (Vector2){ (transform.position.x + agent_offset.x) * unit_to_pixels, (transform.position.y + agent_offset.y) * unit_to_pixels }, unit_to_pixels / agent_texture.width * agent_scale, 1.0f, false);
    }
}
