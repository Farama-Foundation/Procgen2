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

void System_Mob_AI::update(float dt) {
    // Enemy AI
}

void System_Agent::init() {
    agent_texture.load("assets/misc_assets/enemyFloating_1b.png");
}

bool System_Agent::update(float dt, int action) {
    bool alive = true;

    // Parameters
    const float speed = 0.05f;

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

        float movement_x = (agent.action == 1) - (agent.action == 7);
        float movement_y = (agent.action == 5) - (agent.action == 3);

        Vector2 next_velocity = Vector2{ movement_x, movement_y };

        // Change in velocity
        if (dynamics.velocity.x != next_velocity.x || dynamics.velocity.y != next_velocity.y) {
            if (next_velocity.x > 0.0f) {
                if (abs(transform.position.x - (static_cast<int>(transform.position.x) + 0.5f)) <= speed * dt) {
                    Tile_ID id = tilemap->get(static_cast<int>(transform.position.x) + 1, static_cast<int>(transform.position.y)); 

                    if (id != wall) {
                        transform.position.x = static_cast<int>(transform.position.x) + 0.5f;
                        dynamics.velocity = next_velocity;
                    }
                }
            }
            else if (next_velocity.x < 0.0f) {
                if (abs(transform.position.x - (static_cast<int>(transform.position.x) + 0.5f)) <= speed * dt) {
                    Tile_ID id = tilemap->get(static_cast<int>(transform.position.x) - 1, static_cast<int>(transform.position.y)); 

                    if (id != wall) {
                        transform.position.x = static_cast<int>(transform.position.x) + 0.5f;
                        dynamics.velocity = next_velocity;
                    }
                }
            }

            if (next_velocity.y > 0.0f) {
                if (abs(transform.position.y - (static_cast<int>(transform.position.y) + 0.5f)) <= speed * dt) {
                    Tile_ID id = tilemap->get(static_cast<int>(transform.position.x), static_cast<int>(transform.position.y) + 1); 

                    if (id != wall) {
                        transform.position.y = static_cast<int>(transform.position.y) + 0.5f;
                        dynamics.velocity = next_velocity;
                    }
                }
            }
            else if (next_velocity.y < 0.0f) {
                if (abs(transform.position.y - (static_cast<int>(transform.position.y) + 0.5f)) <= speed * dt) {
                    Tile_ID id = tilemap->get(static_cast<int>(transform.position.x), static_cast<int>(transform.position.y) - 1); 

                    if (id != wall) {
                        transform.position.y = static_cast<int>(transform.position.y) + 0.5f;
                        dynamics.velocity = next_velocity;
                    }
                }
            }
        }

        if (dynamics.velocity.x < 0.0f) {
            Tile_ID id = tilemap->get(static_cast<int>(transform.position.x) - 1, static_cast<int>(transform.position.y)); 

            if (id == wall) {
                transform.position.x = static_cast<int>(transform.position.x) + 0.5f;
                dynamics.velocity.x = 0.0f;
            }
        }
        else if (dynamics.velocity.x > 0.0f) {
            Tile_ID id = tilemap->get(static_cast<int>(transform.position.x) + 1, static_cast<int>(transform.position.y)); 

            if (id == wall) {
                transform.position.x = static_cast<int>(transform.position.x) + 0.5f;
                dynamics.velocity.x = 0.0f;
            }
        }

        if (dynamics.velocity.y < 0.0f) {
            Tile_ID id = tilemap->get(static_cast<int>(transform.position.x), static_cast<int>(transform.position.y) - 1); 

            if (id == wall) {
                transform.position.x = static_cast<int>(transform.position.y) + 0.5f;
                dynamics.velocity.x = 0.0f;
            }
        }
        else if (dynamics.velocity.y > 0.0f) {
            Tile_ID id = tilemap->get(static_cast<int>(transform.position.x), static_cast<int>(transform.position.y) + 1); 

            if (id == wall) {
                transform.position.y = static_cast<int>(transform.position.y) + 0.5f;
                dynamics.velocity.y = 0.0f;
            }
        }

        // Move
        transform.position.x += dynamics.velocity.x * speed * dt;
        transform.position.y += dynamics.velocity.y * speed * dt;

        // Camera follows the agent
        gr.camera_position.x = transform.position.x * unit_to_pixels;
        gr.camera_position.y = transform.position.y * unit_to_pixels;
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
        Vector2 agent_offset{ 0.0f, 0.0f };

        Vector2 position{ transform.position.x - 0.25f, transform.position.y - 1.0f };

        gr.render_texture(&agent_texture, (Vector2){ (position.x + agent_offset.x) * unit_to_pixels, (position.y + agent_offset.y) * unit_to_pixels }, unit_to_pixels / agent_texture.width * agent_scale, 1.0f, false);
    }
}
