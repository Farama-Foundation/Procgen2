#pragma once

#include "asset_manager.h"

#include <raylib.h>
#include <stdexcept>

class Asset_Texture {
public:
    Texture2D texture{0};

    // Required
    void load(const std::string &name);

    ~Asset_Texture();
};
