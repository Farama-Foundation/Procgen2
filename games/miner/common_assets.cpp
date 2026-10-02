#include "common_assets.h"
#include "../../cenv/asset_path.h"

void Asset_Texture::load(const std::string &name) {
    const std::string path = procgen2_resolve_asset(name);
    SDL_Surface* surface = IMG_Load(path.c_str());

    if (surface == nullptr)
        throw std::runtime_error("Could not load surface \"" + path + "\"!");

    width = surface->w;
    height = surface->h;

    window_texture = SDL_CreateTextureFromSurface(gr.window_renderer, surface);
    obs_texture = SDL_CreateTextureFromSurface(gr.obs_renderer, surface);

    SDL_DestroySurface(surface);
}

Asset_Texture::~Asset_Texture() {
    if (window_texture != nullptr)
        SDL_DestroyTexture(window_texture);

    if (obs_texture != nullptr)
        SDL_DestroyTexture(obs_texture);
}

Asset_Manager<Asset_Texture> manager_texture;
