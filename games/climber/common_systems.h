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

// --------------------- Mob AI --------------------

class System_Mob_AI : public System {
private:
    int patrol_range = 4;

public:
    bool update(float dt);
};

// --------------------- Player --------------------

static const std::vector<std::string> agent_themes = { "Blue", "Green", "Grey", "Red" };

class System_Agent : public System {
private:
    // Agent textures
    std::vector<Asset_Texture> stand_textures;
    std::vector<Asset_Texture> jump_textures;
    std::vector<Asset_Texture> walk1_textures;
    std::vector<Asset_Texture> walk2_textures;

public:
    void init(); // Needs to load sprites

    // Returns alive status
    void update(float dt, int action);
    void render(int theme);
};
