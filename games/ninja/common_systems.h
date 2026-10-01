#pragma once

#include "common_components.h"
#include "common_assets.h"
#include "ecs.h"

#include <cmath>
#include <algorithm>

enum Sprite_Render_Mode {
    all,
    positive_z,
    negative_z
};

class System_Sprite_Render : public System {
private:
    std::vector<std::pair<float, Entity>> render_entities;

public:
    void update(float dt);
    void render(Sprite_Render_Mode mode);

    void clear_render() {
        render_entities.clear();
    }
};

class System_Hazard : public System {
public:
    std::unordered_set<Entity> &get_entities() {
        return entities;
    }
};

class System_Goal : public System {
public:
    std::unordered_set<Entity> &get_entities() {
        return entities;
    }
};

class System_Projectile : public System {
public:
    std::unordered_set<Entity> &get_entities() {
        return entities;
    }
    void update(float dt);
};

class System_Agent : public System {
private:
    Asset_Texture stand_texture;
    Asset_Texture jump_texture;
    Asset_Texture walk1_texture;
    Asset_Texture walk2_texture;

public:
    void init();

    // Intent + velocity once per env step (charged jump, mixrate).
    void apply_intent(int move_action);

    // Collision/move; returns (alive, reached_goal)
    std::pair<bool, bool> update(float dt, const std::shared_ptr<System_Hazard> &hazard, const std::shared_ptr<System_Goal> &goal);

    void render();
    float jump_charge() const;
    bool facing_right() const;
    Vector2 position() const;

    void try_fire(int special_action, int cur_time);
};
