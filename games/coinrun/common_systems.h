#pragma once

#include "common_components.h"

#include <cmath>
#include <algorithm>
#include <raylib.h>

class System_Sprite_Render : System {
private:
    std::vector<std::pair<float, Entity>> render_entities;

public:
    void update();
};

class System_Tilemap : System {
private:
    std::vector<char> tile_indices;

public:
    void generate();
};
