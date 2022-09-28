#pragma once

#include "common_components.h"
#include "common_assets.h"

#include <cmath>
#include <algorithm>
#include <raylib.h>

// Sprite rendering system
class System_Sprite_Render : System {
private:
    std::vector<std::pair<float, Entity>> render_entities;

public:
    void update(const Camera2D &camera);
};
