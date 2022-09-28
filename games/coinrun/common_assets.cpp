#include "common_assets.h"

void Asset_Texture::load(const std::string &name) {
    texture = LoadTexture(name.c_str());
    
    if (texture.id == 0)
        throw std::runtime_error("Could not load texture!");
}

Asset_Texture::~Asset_Texture() {
    if (texture.id != 0)
        UnloadTexture(texture);
}
