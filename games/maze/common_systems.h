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

// -------------------- Goals --------------------

// Empty mostly, since just need it to collect goals for agent system
class System_Goal : public System {
public:
    std::unordered_set<Entity> &get_entities() {
        return entities;
    }
};

// --------------------- Player --------------------

class System_Agent : public System {
private:
    // Agent textures
    // Asset_Texture stand_texture;
    // Asset_Texture jump_texture;
    // Asset_Texture walk1_texture;
    // Asset_Texture walk2_texture;
    Asset_Texture agent_texture;

public:
    void init(); // Needs to load sprites

    // Returns alive status (false if touched hazard), and whether touched a goal (carrot)
    bool update(float dt, const std::shared_ptr<System_Goal> &goal, int action);
    void render();
};

// ------------------- Particles ------------------

class System_Particles : public System {
private:
    Asset_Texture particle_texture;

public:
    void init(); // Loads sprites
    
    void update(float dt);
    void render();
};
