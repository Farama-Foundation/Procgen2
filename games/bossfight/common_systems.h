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

class System_Mob_AI : public System {
public:
    struct Bullet {
        Vector2 pos;
        Vector2 vel;
        float rotation;
        float frame = -1.0f; // Animation frame, -1.0f is "dead" flag
    };

private:
    std::vector<Asset_Texture> ship_textures;
    std::vector<Asset_Texture*> bullet_textures;
    std::vector<Bullet> bullets;
    std::vector<Asset_Texture*> explosion_textures;
    int next_bullet = 0;
    int num_bullets = 0;
    float bullet_timer = 0.0f;

    int current_ship_texture_index;
    int current_bullet_texture_index;

public:
    void init(); // Needs to load sprites

    void update(float dt);
    void render();

    void reset(std::mt19937 &rng);
};

// --------------------- Player --------------------

class System_Agent : public System {
public:
    struct Bullet {
        Vector2 pos;
        Vector2 vel;
        float rotation;
        float frame = -1.0f; // Animation frame, -1.0f is "dead" flag
    };

private:
    std::vector<Asset_Texture> ship_textures;
    std::vector<Asset_Texture*> bullet_textures;
    std::vector<Bullet> bullets;
    std::vector<Asset_Texture*> explosion_textures;
    int next_bullet = 0;
    int num_bullets = 0;
    float bullet_timer = 0.0f;

    int current_ship_texture_index;
    int current_bullet_texture_index;

public:
    void init(); // Needs to load sprites

    // Returns alive status (false if touched hazard), and whether touched a goal (carrot), and number of hazards destroyed
    bool update(float dt, const std::shared_ptr<System_Hazard> &hazard, int action);
    void render();

    void reset(std::mt19937 &rng);
};
