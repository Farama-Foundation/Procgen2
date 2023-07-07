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
    wall_top,
    wall_mid,
    num_ids
};

enum Collision_Type {
    none = 0,
    full
};

// Tile map system
class System_Tilemap : public System {
public:
    struct Tile {
        std::vector<Asset_Texture> textures;
    };

    struct Config {
        bool easy_mode = false;
    };

private:
    int map_width, map_height;

    std::vector<std::vector<Asset_Texture>> id_to_textures;

    std::vector<Tile_ID> tile_ids;

    void spawn_enemy_mob(int x, int y, std::mt19937 &rng);
    void spawn_point(int x, int y);

public:
    // Initialize the tilemap
    void init();

    // Generate a new random map
    void regenerate(std::mt19937 &rng, const Config &cfg);

    // Set a tile
    void set(int x, int y, Tile_ID id) {
        if (x < 0 || y < 0 || x >= map_width || y >= map_height)
            return;

        tile_ids[y + x * map_height] = id;
    }

    // Top left corner x y, size, id to fill
    void set_area(int x, int y, int width, int height, Tile_ID id);
    void set_area_with_top(int x, int y, int width, int height, Tile_ID mid_id, Tile_ID top_id);

    // Get a tile
    Tile_ID get(int x, int y) {
        if (x < 0 || y < 0 || x >= map_width || y >= map_height)
            return wall_mid; // Out of bounds is a wall

        return tile_ids[y + x * map_height];
    }

    void render(int theme);

    // General collision detection, returns new rectangle position and a collision flag
    std::pair<Vector2, bool> get_collision(Rectangle rectangle, const std::function<Collision_Type(Tile_ID)> &collision_id_func);

    int get_width() const {
        return map_width;
    }

    int get_height() const {
        return map_height;
    }
};
