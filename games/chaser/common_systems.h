#pragma once

#include "common_components.h"
#include "common_assets.h"
#include "ecs.h"

#include <cmath>
#include <algorithm>

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
public:
    void update(float dt);
};

// --------------------- Player --------------------

class System_Agent : public System {
private:
    // Agent textures
    Asset_Texture agent_texture;

    float input_timer = 0.0f; // For timing when input should reset

public:
    void init(); // Needs to load sprites

    // Returns alive status
    bool update(float dt, int action);
    void render();
};
