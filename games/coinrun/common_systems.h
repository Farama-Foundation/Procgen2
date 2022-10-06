#pragma once

#include "common_components.h"
#include "common_assets.h"
#include "ecs.h"

#include <cmath>
#include <algorithm>
#include <raylib.h>

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
    void render(const Rectangle &camera_aabb, Sprite_Render_Mode mode);
};

// --------------------- Mob AI --------------------

class System_Mob_AI : public System {
public:
    void update(float dt);
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

static const std::vector<std::string> agent_themes = { "Beige", "Blue", "Green", "Pink", "Yellow" };

class System_Agent : public System {
private:
    // Agent textures
    std::vector<Asset_Texture> stand_textures;
    std::vector<Asset_Texture> jump_textures;
    std::vector<Asset_Texture> walk1_textures;
    std::vector<Asset_Texture> walk2_textures;

public:
    void init(); // Needs to load sprites

    // Returns alive status (false if touched hazard), and whether touched a goal (coin)
    std::pair<bool, bool> update(float dt, Camera2D &camera, const std::shared_ptr<System_Hazard> &hazard, const std::shared_ptr<System_Goal> &goal, int action);
    void render(int theme);
};
