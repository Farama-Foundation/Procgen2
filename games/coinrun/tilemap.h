#pragma once

#include "common_components.h"
#include "common_assets.h"
#include "helpers.h"
#include "ecs.h"

#include <cmath>
#include <algorithm>
#include <random>
#include <functional>

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

enum Collision_Type {
    none = 0,
    full,
    down_only
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
    std::vector<int> crate_type_indices;
    std::vector<bool> no_collide_mask; // For fallthrough tiles like crates

    void spawn_enemy_saw(int x, int y);
    void spawn_enemy_mob(int x, int y, std::mt19937 &rng);

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

    void render(const Rectangle &camera_aabb, int theme);

    // General collision detection, returns new rectangle position and a collision flag
    std::pair<Vector2, bool> get_collision(Rectangle rectangle, const std::function<Collision_Type(Tile_ID)> &collision_id_func, float velocity_y = 0.0f);

    // For fall-through platforms
    void update_no_collide(const Rectangle &player_rectangle, const Rectangle &outer_rectangle);

    void set_no_collide(int x, int y) {
        if (x < 0 || y < 0 || x >= map_width || y >= map_height)
            return;

        if (get(x, y) == crate)
            no_collide_mask[y + x * map_height] = true;
    }

    int get_width() const {
        return map_width;
    }

    int get_height() const {
        return map_height;
    }
};
