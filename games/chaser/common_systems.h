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

    void clear_render() {
        render_entities.clear();
    }
};

// -------------------- Hazards --------------------

// Empty mostly, since just need it to collect hazards for agent system
class System_Hazard : public System {
public:
    std::unordered_set<Entity> &get_entities() {
        return entities;
    }
};

// Enemy controller
class System_Mob_AI : public System {
private:
    float hatch_timer = 0.0f;
    float anim_timer = 0.0f;
    int anim_index = 0;

    float eat_timer = 0.0f;

    std::vector<Asset_Texture> anim_textures;
    std::array<bool, 4> dir_possibilities;

    static constexpr std::array<Vector2, 4> directions = {
        Vector2{ -1.0f, 0.0f },
        Vector2{ 1.0f, 0.0f },
        Vector2{ 0.0f, -1.0f },
        Vector2{ 0.0f, 1.0f }
    };

public:
    void init();

    void update(float dt, std::mt19937 &rng);

    void eat();
};

// --------------------- Player --------------------

class System_Agent : public System {
public:
    struct Agent_Info {
        Vector2 position = Vector2{ 0.0f, 0.0f };
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

    Agent_Info &get_info() const {
        return info;
    }
};
