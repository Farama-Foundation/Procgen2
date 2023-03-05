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

    Color tint{ 255, 255, 255, 255 };
    float z = 0.0f; // Ordering

    Asset_Texture* texture = nullptr;
};

struct Component_Mob_AI {
    
};

// Reset-triggering
struct Component_Hazard {
    bool destroyable = false;
};

struct Component_Agent {
    int action = 0;
};
