#pragma once

#include "common_components.h"
#include "common_assets.h"
#include "helpers.h"
#include "ecs.h"

#include <cmath>
#include <algorithm>
#include <random>
#include <functional>

enum Distribution_Mode {
    easy_mode,
    hard_mode,
    extreme_mode
};

enum Tile_ID {
    out_of_bounds = -1,
    empty = 0,
    wall,
    marker,
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
        Distribution_Mode mode = extreme_mode;
    };

private:
    int map_width, map_height;

    std::vector<Asset_Texture> id_to_textures;

    std::vector<Tile_ID> tile_ids;

    int total_points = 0;

    void spawn_orb(int tile_index);
    void spawn_point(int tile_index);
    void spawn_egg(int tile_index);

public:
    std::vector<int> free_cells; // Which cells have nothing in them

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
            return out_of_bounds;

        return tile_ids[y + x * map_height];
    }

    void render();

    // General collision detection, returns new rectangle position and a collision flag
    std::pair<Vector2, bool> get_collision(Rectangle rectangle, const std::function<Collision_Type(Tile_ID)> &collision_id_func);

    int get_width() const {
        return map_width;
    }

    int get_height() const {
        return map_height;
    }

    int get_total_points() const {
        return total_points;
    }
};
