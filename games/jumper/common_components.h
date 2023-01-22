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

// Reset-triggering
struct Component_Hazard {};
struct Component_Goal {};

struct Component_Agent {
    int action = 0;
    bool on_ground = false;

    bool face_forward = true;

    // Agent-specific animation
    float rate = 0.1f;
    float t = 0.0f;

    float jump_timer = 0.0f;
    int jumps_left = 2;
};

struct Particle {
    Vector2 position;
    float life = 0.0f;
};

struct Component_Particles {
    std::vector<Particle> particles;

    Vector2 offset{ 0.0f, 0.0f };
    float lifespan = 5.0f;
    float spawn_timer = 0.0f;
    float spawn_time = 0.5f;
    bool enabled = true;
};
