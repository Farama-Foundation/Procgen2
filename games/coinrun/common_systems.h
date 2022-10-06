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

    void update(float dt, Camera2D &camera);
    void render(int theme);
};
