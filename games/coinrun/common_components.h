#pragma once

#include "ecs.h"

#include <raylib.h>

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
    Vector2 acceleration{ 0.0f, 0.0f };
    Vector2 velocity{ 0.0f, 0.0f };
};

// Sprites
struct Component_Sprite {
    // Relative to transform
    Vector2 position{ 0.0f, 0.0f };
    float rotation = 0.0f;
    float scale = 1.0f;

    Color tint{ 255, 255, 255, 255 };
    float z = 0.0f; // Ordering

    Texture2D texture;
};

struct Component_Animation {
    std::vector<Texture2D> frames;

    int frame_index = 0;
    float rate = 0.017f;
    float t = 0.0f;
};

// Reset-triggering
struct Component_Hazard {};

struct Component_Goal {};

// Game logic
struct Component_Sweeper {
    float velocity_x = 0.15f;
};