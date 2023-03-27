#pragma once

#include "ecs.h"
#include "common_assets.h"

// General
struct Component_Transform {
    Vector2 position{ 0.0f, 0.0f };
    float rotation = 0.0f;
    float scale = 1.0f;
};

// Physics
struct Component_Collision {
    Rectangle bounds{ -0.5f, -0.5f, 1.0f, 1.0f };
};

struct Component_Dynamics {
    Vector2 velocity{ 0.0f, 0.0f };
};

// Sprites
struct Component_Sprite {
    // Relative to transform
    Vector2 position{ 0.0f, 0.0f };
    float rotation = 0.0f;
    float scale = 1.0f;
    bool flip_x = false;
    bool flip_y = false;

    Color tint{ 255, 255, 255, 255 };
    float z = 0.0f; // Ordering

    Asset_Texture* texture = nullptr;
};

struct Component_Mob_AI {
    float phase_timer = 0.0f;
    int phase_index = 0;
    int weapon_index = 0;

    float attack_timer = 0.0f;

    int hp = 0;
};

// Reset-triggering
struct Component_Hazard {
    bool destroyable = false;
};

struct Component_Agent {
    int action = 0;
};
