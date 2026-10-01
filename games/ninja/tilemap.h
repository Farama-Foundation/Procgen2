#pragma once

#include "common_components.h"
#include "common_assets.h"
#include "helpers.h"
#include "ecs.h"

#include <cmath>
#include <algorithm>
#include <random>
#include <functional>

enum Tile_ID {
    empty = 0,
    wall_mid,
    fire_tile,
    bomb_tile,
    num_ids
};

enum Collision_Type {
    none = 0,
    full
};

static const std::vector<std::string> wall_themes = {
    "assets/misc_assets/tile_bricksGrey.png",
    "assets/misc_assets/tile_bricksGrown.png",
    "assets/misc_assets/tile_bricksRed.png"
};

class System_Tilemap : public System {
public:
    struct Config {};

private:
    int map_width = 0, map_height = 0;
    std::vector<std::vector<Asset_Texture>> id_to_textures;
    std::vector<Tile_ID> tile_ids;

public:
    void init();
    void regenerate(std::mt19937 &rng, const Config &cfg);

    void set(int x, int y, Tile_ID id) {
        if (x < 0 || y < 0 || x >= map_width || y >= map_height)
            return;
        tile_ids[y + x * map_height] = id;
    }

    void set_area(int x, int y, int width, int height, Tile_ID id);

    Tile_ID get(int x, int y) {
        if (x < 0 || y < 0 || x >= map_width || y >= map_height)
            return wall_mid;
        return tile_ids[y + x * map_height];
    }

    void render(int theme);

    std::pair<Vector2, bool> get_collision(Rectangle rectangle, const std::function<Collision_Type(Tile_ID)> &collision_id_func);

    int get_width() const { return map_width; }
    int get_height() const { return map_height; }

    // Original Y-up center -> this engine's Y-down world Y
    float world_y_from_orig(float orig_y) const { return static_cast<float>(map_height) - orig_y; }
};
