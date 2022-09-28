#pragma once

#include "common_components.h"
#include "common_assets.h"

#include <cmath>
#include <algorithm>
#include <raylib.h>

// Tile map system
class System_Tilemap : System {
private:
    int width, height;

    std::vector<Asset_Texture> textures;

    std::vector<char> tile_indices;

public:
    // Initialize the tilemap
    void init();

    // Generate a new random map
    void regenerate(int width, int height);
};

