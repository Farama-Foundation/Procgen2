#pragma once

#include "asset_manager.h"

#include "renderer.h"

#include <stdexcept>

class Asset_Texture {
public:
    // SDL requires different textures for different renders for some reason
    SDL_Texture* obs_texture = nullptr;
    SDL_Texture* window_texture = nullptr;

    int width = 0;
    int height = 0;

    // Required
    void load(const std::string &name);

    ~Asset_Texture();
};

// Manager for all textures
extern Asset_Manager<Asset_Texture> manager_texture;
