#pragma once

#include "common_components.h"
#include "common_assets.h"
#include "helpers.h"

#include <cmath>
#include <algorithm>
#include <random>

#include <raylib.h>

enum Tile_ID {
    empty = 0,
    wall_top,
    wall_mid,
    lava_top,
    lava_mid,
    crate,
    num_ids
};

static const std::vector<std::string> wall_themes = { "Dirt", "Grass", "Planet", "Sand", "Snow", "Stone" };
static const std::vector<std::string> walking_enemies = { "slimeBlock", "slimePurple", "slimeBlue", "slimeGreen", "mouse", "snail", "ladybug", "wormGreen", "wormPink" };
static const std::vector<std::string> crate_types = { "boxCrate", "boxCrate_double", "boxCrate_single", "boxCrate_warning" };

// Tile map system
class System_Tilemap : public System {
public:
    struct Tile {
        std::vector<Asset_Texture> textures;
    };

    struct Config {
        bool easy_mode = false;
        bool allow_pit = true;
        bool allow_crate = true;
        bool allow_dy = true;
        bool allow_mobs = true;
    };

private:
    int map_width, map_height;

    std::vector<std::vector<Asset_Texture>> id_to_textures;

    std::vector<Tile_ID> tile_ids;

public:
    // Initialize the tilemap
    void init();

    // Generate a new random map
    void regenerate(std::mt19937 &rng, const Config &cfg);

    void set(int x, int y, Tile_ID id) {
        if (x < 0 || y < 0 || x >= map_width || y >= map_height)
            return;

        tile_ids[y + x * map_height] = id;
    }

    // top left corner x y, size, id to fill
    void set_area(int x, int y, int width, int height, Tile_ID id);
    void set_area_with_top(int x, int y, int width, int height, Tile_ID mid_id, Tile_ID top_id);

    Tile_ID get(int x, int y) {
        if (x < 0 || y < 0 || x >= map_width || y >= map_height)
            return empty;

        return tile_ids[y + x  * map_height];
    }

    void render(const Rectangle &camera_aabb, int theme);
};

