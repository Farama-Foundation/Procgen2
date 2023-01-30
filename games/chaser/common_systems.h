#pragma once

#include "common_components.h"
#include "common_assets.h"
#include "ecs.h"

#include <cmath>
#include <algorithm>
#include <random>

// -------------------- Sprites ---------------------
//
// Selector for sprites to render
enum Sprite_Render_Mode {
    all,
    positive_z,
    negative_z
};

// Sprite rendering system
class System_Sprite_Render : public System {
private:
    std::vector<std::pair<float, Entity>> render_entities;

public:
    void update(float dt);
    void render(Sprite_Render_Mode mode);

    void reset() {
        render_entities.clear();
    }
};

// Empty mostly, since just need it to collect points for agent system
class System_Point : public System {
public:
    int num_points_collected = 0;
    int point_delta = 0;
    int num_points_available = 0;

    void update();

    void reset() {
        num_points_collected = 0;
        point_delta = 0;
        num_points_available = 0;
    }
};

// Enemy controller
class System_Mob_AI : public System {
private:
    float anim_timer = 0.0f;
    int anim_index = 0;

    float eat_timer = 0.0f;

    std::vector<Asset_Texture> anim_textures;
    std::array<bool, 4> dir_possibilities;

    const std::array<Vector2, 4> directions = {
        Vector2{ -1.0f, 0.0f },
        Vector2{ 1.0f, 0.0f },
        Vector2{ 0.0f, -1.0f },
        Vector2{ 0.0f, 1.0f }
    };

public:
    void init();

    bool update(float dt, std::mt19937 &rng); // Return true if player hits enemy while vulnerable

    void eat();

    void reset() {
        anim_timer = 0.0f;
        anim_index = 0;
        eat_timer = 0.0f;
    }
};

// --------------------- Player --------------------

class System_Agent : public System {
public:
    struct Agent_Info {
        Entity entity;
    };

private:
    Agent_Info info;

    // Agent textures
    Asset_Texture agent_texture;

    float input_timer = 0.0f; // For timing when input should reset

public:
    void init(); // Needs to load sprites

    // Returns alive status
    bool update(float dt, int action);
    void render();

    const Agent_Info &get_info() const {
        return info;
    }

    void reset() {
        input_timer = 0.0f;
    }
};
