#include "renderer.h"

#include "common_assets.h"

void Renderer::render_texture(Asset_Texture* texture, const Vector2 &position, float scale, float alpha, bool flip_horizontal) {
    SDL_Renderer* renderer = get_renderer();

    SDL_FRect src_rect{ 0.0f, 0.0f, static_cast<float>(texture->width), static_cast<float>(texture->height) };

    SDL_FRect dst_rect{ (position.x - camera_position.x) * camera_scale + camera_size.x * 0.5f, (position.y - camera_position.y) * camera_scale + camera_size.y * 0.5f,
        texture->width * scale * camera_scale, texture->height * scale * camera_scale };

    // Culling
    if (dst_rect.x > camera_size.x || dst_rect.y >= camera_size.y || dst_rect.x + dst_rect.w < 0 || dst_rect.y + dst_rect.h < 0)
        return;

    // Shrink if needed to avoid overdraw on large textures
    if (dst_rect.x < 0.0f) {
        float ratio = -dst_rect.x / dst_rect.w; 

        src_rect.x += src_rect.w * ratio;
        src_rect.w -= src_rect.x;

        dst_rect.w += dst_rect.x;
        dst_rect.x = 0.0f; 
    }
        
    if (dst_rect.x + dst_rect.w > camera_size.x) {
        float ratio = (dst_rect.x + dst_rect.w - camera_size.x) / dst_rect.w; 
        
        src_rect.w = src_rect.w * (1.0f - ratio);

        dst_rect.w = camera_size.x - dst_rect.x;
    }
    
    if (dst_rect.y < 0.0f) {
        float ratio = -dst_rect.y / dst_rect.h; 

        src_rect.y += src_rect.h * ratio;
        src_rect.h -= src_rect.y;

        dst_rect.h += dst_rect.y;
        dst_rect.y = 0.0f; 
    }
        
    if (dst_rect.y + dst_rect.h > camera_size.y) {
        float ratio = (dst_rect.y + dst_rect.h - camera_size.y) / dst_rect.h; 
        
        src_rect.h = src_rect.h * (1.0f - ratio);

        dst_rect.h = camera_size.y - dst_rect.y;
    }

    if (alpha != 1.0f)
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255 * alpha);

    SDL_Rect src_recti{ static_cast<int>(std::floor(src_rect.x)), static_cast<int>(std::floor(src_rect.y)), static_cast<int>(std::ceil(src_rect.w)) + 1, static_cast<int>(std::ceil(src_rect.h)) + 1 };

    // Prevent flickering from integer src_rect values by compensating
    Vector2 offset{ src_rect.x - src_recti.x, src_rect.y - src_recti.y };
    Vector2 size_ratio{ src_recti.w / src_rect.w, src_recti.h / src_rect.h };

    dst_rect.w *= size_ratio.x;
    dst_rect.h *= size_ratio.y;
    dst_rect.x -= offset.x * (dst_rect.w / src_rect.w);
    dst_rect.y -= offset.y * (dst_rect.h / src_rect.h);

    SDL_RenderCopyExF(renderer, rendering_obs ? texture->obs_texture : texture->window_texture, &src_recti, &dst_rect, 0.0f, NULL, flip_horizontal ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);

    if (alpha != 1.0f)
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
}

Renderer::~Renderer() {
}

Renderer gr;
