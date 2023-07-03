#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_image.h>

#include "helpers.h"

class Asset_Texture;

class Renderer {
public:
    bool rendering_obs = false;

    SDL_Renderer* window_renderer = nullptr;
    SDL_Renderer* obs_renderer = nullptr;

    // Camera
    Vector2 camera_position{ 0 };
    Vector2 camera_size{ 64, 64 };
    float camera_scale = 1.0f;

    void render_texture(Asset_Texture* texture, const Vector2 &position, float scale = 1.0f, float alpha = 1.0f, bool flip_horizontal = false, bool flip_vertical = false);
    void render_texture_rotated(Asset_Texture* texture, const Vector2 &position, float rotation, float scale = 1.0f, float alpha = 1.0f);

    SDL_Renderer* get_renderer() const {
        return rendering_obs ? obs_renderer : window_renderer;
    }

    ~Renderer();
};

extern Renderer gr; // Global renderer

