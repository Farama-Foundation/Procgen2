#include "common_systems.h"

#include "tilemap.h"
#include "helpers.h"

#include <cmath>
#include <cassert>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void System_Sprite_Render::update(float dt) {
    if (render_entities.size() != entities.size())
        render_entities.resize(entities.size());

    int index = 0;
    for (auto const &e : entities) {
        auto &sprite = c.get_component<Component_Sprite>(e);
        if (c.entity_manager.get_signature(e)[c.component_manager.get_component_type<Component_Animation>()]) {
            auto &animation = c.get_component<Component_Animation>(e);
            animation.t += dt;
            int frames_advance = animation.t * animation.rate;
            animation.t -= frames_advance / animation.rate;
            animation.frame_index = (animation.frame_index + frames_advance) % (int)animation.frames.size();
            sprite.texture = animation.frames[animation.frame_index];
        }
        render_entities[index] = std::make_pair(sprite.z, e);
        index++;
    }

    std::sort(render_entities.begin(), render_entities.end(), [](const std::pair<float, Entity> &left, const std::pair<float, Entity> &right) {
        return left.first < right.first;
    });
}

void System_Sprite_Render::render(Sprite_Render_Mode mode) {
    for (size_t i = 0; i < render_entities.size(); i++) {
        Entity e = render_entities[i].second;
        auto const &sprite = c.get_component<Component_Sprite>(e);
        auto const &transform = c.get_component<Component_Transform>(e);
        if (sprite.texture == nullptr)
            continue;
        if (mode == positive_z && sprite.z < 0.0f)
            continue;
        else if (mode == negative_z && sprite.z >= 0.0f)
            break;

        float scale = transform.scale * sprite.scale;
        gr.render_texture(sprite.texture,
            (Vector2){ (transform.position.x + sprite.position.x) * unit_to_pixels,
                       (transform.position.y + sprite.position.y) * unit_to_pixels },
            scale * unit_to_pixels / sprite.texture->width, 1.0f, sprite.flip_x);
    }
}

void System_Projectile::update(float dt) {
    std::shared_ptr<System_Tilemap> tilemap = c.system_manager.get_system<System_Tilemap>();
    std::vector<Entity> to_destroy;
    std::vector<Vector2> explosions;

    for (auto const &e : entities) {
        auto &proj = c.get_component<Component_Projectile>(e);
        auto &transform = c.get_component<Component_Transform>(e);
        auto &dynamics = c.get_component<Component_Dynamics>(e);

        proj.expire -= dt;
        if (proj.expire <= 0.0f) {
            to_destroy.push_back(e);
            continue;
        }

        if (!proj.is_star)
            continue;

        transform.position.x += dynamics.velocity.x * dt;
        transform.position.y += dynamics.velocity.y * dt;

        const auto &collision = c.get_component<Component_Collision>(e);
        Rectangle world{ transform.position.x + collision.bounds.x,
                         transform.position.y + collision.bounds.y,
                         collision.bounds.width, collision.bounds.height };

        // Inclusive ceil() on a flush AABB pulls in the floor tile the ninja is standing on.
        int lower_x = (int)std::floor(world.x);
        int lower_y = (int)std::floor(world.y);
        int upper_x = (int)std::floor(world.x + world.width - 1e-4f);
        int upper_y = (int)std::floor(world.y + world.height - 1e-4f);

        bool hit = false;
        for (int y = lower_y; y <= upper_y && !hit; y++) {
            for (int x = lower_x; x <= upper_x && !hit; x++) {
                int ty = tilemap->get_height() - 1 - y;
                Tile_ID id = tilemap->get(x, ty);
                if (id == bomb_tile) {
                    tilemap->set(x, ty, empty);
                    explosions.push_back(Vector2{ x + 0.5f, tilemap->world_y_from_orig(ty + 0.5f) });
                    hit = true;
                } else if (id == wall_mid) {
                    hit = true;
                }
            }
        }
        if (hit)
            to_destroy.push_back(e);
    }

    for (Entity e : to_destroy)
        c.destroy_entity(e);

    for (const auto &p : explosions) {
        Entity ex = c.create_entity();
        c.add_component(ex, Component_Transform{ .position{ p } });
        c.add_component(ex, Component_Sprite{ .position{ -0.5f, -0.5f }, .z = 2.0f,
            .texture = &manager_texture.get("assets/misc_assets/explosion1.png") });
        c.add_component(ex, Component_Collision{ .bounds{ -0.5f, -0.5f, 1.0f, 1.0f } });
        c.add_component(ex, Component_Hazard{});
        c.add_component(ex, Component_Dynamics{});
        c.add_component(ex, Component_Projectile{ .expire = 5.0f, .is_star = false });
    }
}

void System_Agent::init() {
    stand_texture.load("assets/platformer/zombie_idle.png");
    jump_texture.load("assets/platformer/zombie_jump.png");
    walk1_texture.load("assets/platformer/zombie_walk1.png");
    walk2_texture.load("assets/platformer/zombie_walk2.png");
}

void System_Agent::apply_intent(int move_action) {
    const float mixrate = 0.5f;
    const float air_control = 0.15f;
    const float maxspeed = 0.5f;
    const float max_jump = 1.5f;
    const float gravity = 0.2f;
    const float jump_charge_inc = 0.25f;

    std::shared_ptr<System_Tilemap> tilemap = c.system_manager.get_system<System_Tilemap>();

    assert(entities.size() == 1);
    for (auto const &e : entities) {
        auto &agent = c.get_component<Component_Agent>(e);
        auto &dynamics = c.get_component<Component_Dynamics>(e);
        auto const &transform = c.get_component<Component_Transform>(e);

        float action_vx = (float)(move_action / 3 - 1);
        float action_vy = (float)(move_action % 3 - 1);
        if (action_vy < 0.0f)
            action_vy = 0.0f;

        if (action_vx > 0.0f)
            agent.face_forward = true;
        if (action_vx < 0.0f)
            agent.face_forward = false;

        // Original samples tiles just below the agent (Y-up). Here +Y is down.
        const float rx = 0.5f, ry = 0.5f;
        auto tile_at = [&](float wx, float wy) -> Tile_ID {
            int tx = (int)std::floor(wx);
            int ty = tilemap->get_height() - 1 - (int)std::floor(wy);
            return tilemap->get(tx, ty);
        };
        bool has_support =
            tile_at(transform.position.x - (rx - 0.01f), transform.position.y + (ry + 0.01f)) == wall_mid ||
            tile_at(transform.position.x + (rx - 0.01f), transform.position.y + (ry + 0.01f)) == wall_mid;
        agent.on_ground = has_support;

        if (has_support && action_vy == 1.0f) {
            agent.jump_charge += jump_charge_inc;
            if (agent.jump_charge > 1.0f)
                agent.jump_charge = 1.0f;
        } else {
            action_vy = 0.0f;
        }

        if (!has_support)
            agent.jump_charge = 0.0f;

        float mixrate_x = has_support ? mixrate : (mixrate * air_control);
        dynamics.velocity.x = (1.0f - mixrate_x) * dynamics.velocity.x + mixrate_x * maxspeed * action_vx;

        if (action_vy < 1.0f && agent.jump_charge > 0.0f) {
            dynamics.velocity.y = -agent.jump_charge * max_jump;
            agent.jump_charge = 0.0f;
        }

        if (!has_support) {
            if (dynamics.velocity.y < 2.0f)
                dynamics.velocity.y += gravity;
        }
    }
}

std::pair<bool, bool> System_Agent::update(float dt, const std::shared_ptr<System_Hazard> &hazard, const std::shared_ptr<System_Goal> &goal) {
    bool alive = true;
    bool achieved_goal = false;
    std::shared_ptr<System_Tilemap> tilemap = c.system_manager.get_system<System_Tilemap>();

    assert(entities.size() == 1);
    for (auto const &e : entities) {
        auto &agent = c.get_component<Component_Agent>(e);
        auto &transform = c.get_component<Component_Transform>(e);
        auto &dynamics = c.get_component<Component_Dynamics>(e);
        const auto &collision = c.get_component<Component_Collision>(e);

        transform.position.x += dynamics.velocity.x * dt;
        transform.position.y += dynamics.velocity.y * dt;

        Rectangle world_collision{ transform.position.x + collision.bounds.x, transform.position.y + collision.bounds.y,
                                   collision.bounds.width, collision.bounds.height };

        auto collision_data = tilemap->get_collision(world_collision, [](Tile_ID id) -> Collision_Type {
            return (id == wall_mid ? full : none);
        });

        Vector2 delta_position{ collision_data.first.x - world_collision.x, collision_data.first.y - world_collision.y };
        agent.on_ground = delta_position.y < 0.0f && collision_data.second;

        transform.position.x = collision_data.first.x - collision.bounds.x;
        transform.position.y = collision_data.first.y - collision.bounds.y;

        world_collision.x = transform.position.x + collision.bounds.x;
        world_collision.y = transform.position.y + collision.bounds.y;

        if (delta_position.x != 0.0f)
            dynamics.velocity.x = 0.0f;
        if (delta_position.y > 0.0f && collision_data.second)
            dynamics.velocity.y = 0.0f;
        if (agent.on_ground)
            dynamics.velocity.y = 0.0f;

        auto fire_hit = tilemap->get_collision(world_collision, [](Tile_ID id) -> Collision_Type {
            return (id == fire_tile || id == bomb_tile ? full : none);
        });
        if (fire_hit.second)
            alive = false;

        for (auto const &h : hazard->get_entities()) {
            auto const &ht = c.get_component<Component_Transform>(h);
            auto const &hc = c.get_component<Component_Collision>(h);
            Rectangle hw{ ht.position.x + hc.bounds.x, ht.position.y + hc.bounds.y, hc.bounds.width, hc.bounds.height };
            if (check_collision(world_collision, hw)) {
                alive = false;
                break;
            }
        }

        for (auto const &g : goal->get_entities()) {
            auto const &gt = c.get_component<Component_Transform>(g);
            auto const &gc = c.get_component<Component_Collision>(g);
            Rectangle gw{ gt.position.x + gc.bounds.x, gt.position.y + gc.bounds.y, gc.bounds.width, gc.bounds.height };
            if (check_collision(world_collision, gw)) {
                achieved_goal = true;
                break;
            }
        }

        gr.camera_position.x = transform.position.x * unit_to_pixels;
        gr.camera_position.y = transform.position.y * unit_to_pixels;

        agent.t += dt;
        agent.t = std::fmod(agent.t, 1.0f);
    }

    return std::make_pair(alive, achieved_goal);
}

void System_Agent::try_fire(int special_action, int cur_time) {
    if (special_action <= 0)
        return;

    assert(entities.size() == 1);
    for (auto const &e : entities) {
        auto &agent = c.get_component<Component_Agent>(e);
        if ((cur_time - agent.last_fire_time) < 3)
            continue;

        const auto &transform = c.get_component<Component_Transform>(e);

        float theta = 0.0f;
        if (special_action == 1)
            theta = 0.0f;
        else if (special_action == 2)
            theta = (float)M_PI / 4.0f;
        else if (special_action == 3)
            theta = (float)M_PI / 2.0f;
        else if (special_action == 4)
            theta = -(float)M_PI / 4.0f;
        else
            continue;

        if (!agent.face_forward)
            theta = (float)M_PI - theta;

        // Original Y-up sin(theta); engine Y-down so negate vy
        float bullet_vel = 1.0f;
        float vx = bullet_vel * std::cos(theta);
        float vy = -bullet_vel * std::sin(theta);

        Vector2 spawn{ transform.position.x + vx * 0.55f, transform.position.y + vy * 0.55f };

        Entity star = c.create_entity();
        c.add_component(star, Component_Transform{ .position{ spawn } });
        c.add_component(star, Component_Sprite{ .position{ -0.25f, -0.25f }, .scale = 0.5f, .z = 2.0f,
            .texture = &manager_texture.get("assets/misc_assets/saw.png") });
        c.add_component(star, Component_Collision{ .bounds{ -0.25f, -0.25f, 0.5f, 0.5f } });
        c.add_component(star, Component_Dynamics{ .velocity{ vx, vy } });
        c.add_component(star, Component_Projectile{ .expire = 15.0f, .is_star = true });

        agent.last_fire_time = cur_time;
    }
}

void System_Agent::render() {
    assert(entities.size() == 1);
    for (auto const &e : entities) {
        auto const &agent = c.get_component<Component_Agent>(e);
        auto const &transform = c.get_component<Component_Transform>(e);
        auto const &dynamics = c.get_component<Component_Dynamics>(e);

        Asset_Texture* texture;
        if (std::abs(dynamics.velocity.x) < 0.01f && agent.on_ground)
            texture = &stand_texture;
        else if (!agent.on_ground)
            texture = &jump_texture;
        else if (agent.t > 0.5f)
            texture = &walk2_texture;
        else
            texture = &walk1_texture;

        Vector2 position{ transform.position.x - 0.5f, transform.position.y - 0.5f };
        gr.render_texture(texture, (Vector2){ position.x * unit_to_pixels, position.y * unit_to_pixels },
            unit_to_pixels / texture->width, 1.0f, !agent.face_forward);
    }
}

float System_Agent::jump_charge() const {
    for (auto const &e : entities)
        return c.get_component<Component_Agent>(e).jump_charge;
    return 0.0f;
}

bool System_Agent::facing_right() const {
    for (auto const &e : entities)
        return c.get_component<Component_Agent>(e).face_forward;
    return true;
}

Vector2 System_Agent::position() const {
    for (auto const &e : entities)
        return c.get_component<Component_Transform>(e).position;
    return Vector2{ 0, 0 };
}
