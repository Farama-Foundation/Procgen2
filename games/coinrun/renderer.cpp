#include "renderer.h"

#include "common_assets.h"

void Renderer::render_texture(Asset_Texture* texture, const Vector2 &position, float scale, float alpha, bool flip_horizontal) {
    SDL_Renderer* renderer = get_renderer();

    SDL_Rect src_rect{ 0, 0, texture->width, texture->height };

    SDL_Rect dst_rect{ static_cast<int>((position.x - camera_position.x) * camera_scale + camera_size.x * 0.5f), static_cast<int>((position.y - camera_position.y) * camera_scale + camera_size.y * 0.5f),
        static_cast<int>(texture->width * scale * camera_scale), static_cast<int>(texture->height * scale * camera_scale) };

    // Culling
    if (dst_rect.x > camera_size.x || dst_rect.y >= camera_size.y || dst_rect.x + dst_rect.w < 0 || dst_rect.y + dst_rect.h < 0)
        return;

    // Shrink if needed to avoid overdraw on large textures
    if (dst_rect.x < 0) {
        float ratio = static_cast<float>(-dst_rect.x) / static_cast<float>(dst_rect.w); 

        src_rect.x += static_cast<int>(std::ceil(src_rect.w * ratio));
        src_rect.w -= src_rect.x;

        dst_rect.w += dst_rect.x;
        dst_rect.x = 0; 
    }
        
    if (dst_rect.x + dst_rect.w > camera_size.x) {
        float ratio = static_cast<float>(dst_rect.x + dst_rect.w - camera_size.x) / static_cast<float>(dst_rect.w); 
        
        src_rect.w = static_cast<int>(std::ceil(src_rect.w * (1.0f - ratio)));

        dst_rect.w = camera_size.x - dst_rect.x;
    }

    if (dst_rect.y < 0) {
        float ratio = static_cast<float>(-dst_rect.y) / static_cast<float>(dst_rect.h); 

        src_rect.y += static_cast<int>(std::ceil(src_rect.h * ratio));
        src_rect.h -= src_rect.y;

        dst_rect.h += dst_rect.y;
        dst_rect.y = 0; 
    }
        
    if (dst_rect.y + dst_rect.h > camera_size.y) {
        float ratio = static_cast<float>(dst_rect.y + dst_rect.h - camera_size.y) / static_cast<float>(dst_rect.h); 
        
        src_rect.h = static_cast<int>(std::ceil(src_rect.h * (1.0f - ratio)));

        dst_rect.h = camera_size.y - dst_rect.y;
    }
    
    if (alpha != 1.0f)
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255 * alpha);

    SDL_RenderCopyEx(renderer, rendering_obs ? texture->obs_texture : texture->window_texture, &src_rect, &dst_rect, 0.0f, NULL, flip_horizontal ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);

    if (alpha != 1.0f)
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
}

Renderer::~Renderer() {
}

Renderer gr;
