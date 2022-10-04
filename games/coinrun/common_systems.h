#pragma once

#include "common_components.h"
#include "common_assets.h"
#include "ecs.h"

#include <cmath>
#include <algorithm>
#include <raylib.h>

// Selector for sprites to render
enum Sprite_Render_Mode {
    all,
    positive_z,
    negative_z
};

// Sprite rendering system
class System_Sprite_Render : public System {
private:
    std::vector<std::pair<float, Entity>> render_entities;

public:
    void update(float dt);
    void render(const Rectangle &camera_aabb, Sprite_Render_Mode mode);
};

