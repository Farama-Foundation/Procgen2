#include "renderer.h"

#include "common_assets.h"

void Renderer::render_texture(Asset_Texture* texture, const Vector2 &position, float scale, float alpha, bool flip_horizontal) {
    SDL_Renderer* renderer = get_renderer();

    SDL_Rect dst_rect{ static_cast<int>((position.x - camera_position.x) * camera_scale + camera_size.x * 0.5f), static_cast<int>((position.y - camera_position.y) * camera_scale + camera_size.y * 0.5f),
        static_cast<int>(texture->width * scale * camera_scale), static_cast<int>(texture->height * scale * camera_scale) };

    // Culling
    if (dst_rect.x > camera_size.x || dst_rect.y >= camera_size.y || dst_rect.x + dst_rect.w < 0 || dst_rect.y + dst_rect.h < 0)
        return;

    if (alpha != 1.0f)
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255 * alpha);

    SDL_RenderCopyEx(renderer, rendering_obs ? texture->obs_texture : texture->window_texture, NULL, &dst_rect, 0.0f, NULL, flip_horizontal ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);

    if (alpha != 1.0f)
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
}

Renderer::~Renderer() {
}

Renderer gr;
