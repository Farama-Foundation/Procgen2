#include "tilemap.h"

#include "maze_generator.h"
#include "room_generator.h"

void System_Tilemap::init() {
    id_to_textures.resize(num_ids);

    // Load textures
    id_to_textures[wall].resize(1);

    id_to_textures[wall][0].load("assets/misc_assets/tileStone_slope.png");

    // Pre-load
    manager_texture.get("assets/misc_assets/yellowCrystal.png");
    manager_texture.get("assets/misc_assets/enemySpikey_1b.png");
}

// Tile manipulation
void System_Tilemap::set_area(int x, int y, int width, int height, Tile_ID id) {
    for (int dx = 0; dx < width; dx++)
        for (int dy = 0; dy < height; dy++)
            set(x + dx, y + dy, id);
}

void System_Tilemap::set_area_with_top(int x, int y, int width, int height, Tile_ID mid_id, Tile_ID top_id) {
    set_area(x, y, width, height - 1, mid_id);
    set_area(x, y + height - 1, width, 1, top_id);
}

void System_Tilemap::spawn_orb(int tile_index) {
    int x = tile_index / map_height;
    int y = tile_index % map_height;

    Entity e = c.create_entity();

    Vector2 pos = { static_cast<float>(x) + 0.5f, static_cast<float>(map_height - 1 - y) + 0.5f };

    Asset_Texture* texture = &manager_texture.get("assets/misc_assets/yellowCrystal.png");

    c.add_component(e, Component_Transform{ .position{ pos } });
    c.add_component(e, Component_Sprite{ .position{ -0.5f, -0.5f }, .scale=0.4f, .texture = texture });
    c.add_component(e, Component_Collision{ .bounds{ -0.5f, -0.5f, 1.0f, 1.0f }});
}

void System_Tilemap::spawn_egg(int tile_index) {
    int x = tile_index / map_height;
    int y = tile_index % map_height;

    Entity e = c.create_entity();

    Vector2 pos = { static_cast<float>(x) + 0.5f, static_cast<float>(map_height - 1 - y) + 0.5f };

    Asset_Texture* texture = &manager_texture.get("assets/misc_assets/enemySpikey_1b.png");

    c.add_component(e, Component_Transform{ .position{ pos } });
    c.add_component(e, Component_Sprite{ .position{ -0.5f, -0.5f }, .scale=0.4f, .texture = texture });
    c.add_component(e, Component_Collision{ .bounds{ -0.5f, -0.5f, 1.0f, 1.0f }});
    c.add_component(e, Component_Mob_AI{});
}

// Main map generation
void System_Tilemap::regenerate(std::mt19937 &rng, const Config &cfg) {
    int world_dim;
    int total_enemies;
    int extra_orb_sign;

    if (cfg.mode == hard_mode) {
        world_dim = 13;
        total_enemies = 3;
        extra_orb_sign = -1;
    }
    else if (cfg.mode == extreme_mode) {
        world_dim = 19;
        total_enemies = 5;
        extra_orb_sign = 1;
    }
    else { // Easy
        world_dim = 11;
        total_enemies = 3;
        extra_orb_sign = 0;
    }

    const int main_width = world_dim;
    const int main_height = world_dim;

    this->map_width = main_width;
    this->map_height = main_height;

    tile_ids.resize(map_width * map_height);

    // Clear
    std::fill(tile_ids.begin(), tile_ids.end(), empty);

    // Generate maze with no dead ends
    Maze_Generator maze_generator;
    maze_generator.generate_maze_no_dead_ends(world_dim, world_dim, rng);

    free_cells.clear();

    std::vector<std::vector<int>> quadrants;
    std::vector<int> orbs_for_quadrant;

    int num_quadrants = 4;

    std::uniform_int_distribution<int> quadDist(0, num_quadrants - 1);
    int extra_quad = quadDist(rng);

    for (int i = 0; i < num_quadrants; i++) {
        std::vector<int> quadrant;

        orbs_for_quadrant.push_back(1 + (i == extra_quad ? extra_orb_sign : 0));
        quadrants.push_back(quadrant);
    }

    for (int x = 0; x < main_width; x++)
        for (int y = 0; y < main_height; y++) {
            int obj = maze_generator.get(x + 1, y + 1);

            set(x, y, obj == 1 ? wall : empty);

            if (obj == 0) {
                int index = y + x * map_height;

                free_cells.push_back(index);

                int quad_index = (x >= map_width / 2) * 2 + (y >= map_height / 2);

                quadrants[quad_index].push_back(index);
            }
        }

    for (int i = 0; i < num_quadrants; i++) {
        int num_orbs = orbs_for_quadrant[i];

        std::vector<int> quadrant = quadrants[i];

        // Choose randomly without overlap
        std::uniform_int_distribution<int> pos_dist(0, quadrant.size() - 1);

        std::unordered_set<int> selected_indices;

        for (int j = 0; j < num_orbs; j++) {
            int pos = pos_dist(rng);

            while (std::find(selected_indices.begin(), selected_indices.end(), pos) == selected_indices.end())
                pos = (pos + 1) % quadrant.size();

            selected_indices.insert(pos);
        }

        for (int j : selected_indices) {
            int cell = quadrant[j];

            spawn_orb(cell);

            tile_ids[cell] = marker;
        }
    }

    for (int i = 0; i < tile_ids.size(); i++) {
        if (tile_ids[i] == empty)
            free_cells.push_back(i);
    }

    // Choose randomly without overlap
    std::uniform_int_distribution<int> pos_dist(0, free_cells.size() - 1);

    std::unordered_set<int> selected_indices;

    for (int j = 0; j < total_enemies + 1; j++) {
        int pos = pos_dist(rng);

        while (std::find(selected_indices.begin(), selected_indices.end(), pos) == selected_indices.end())
            pos = (pos + 1) % free_cells.size();

        selected_indices.insert(pos);
    }

    auto it = selected_indices.begin();

    int start_index = *it;
    int start = free_cells[start_index];

    int agent_spawn_x = start / map_height;
    int agent_spawn_y = start % map_height;

    for (int i = 0; i < total_enemies; i++) {
        it++;

        int cell = free_cells[*it];

        tile_ids[cell] = marker;

        spawn_egg(cell);
    }

    for (int cell : free_cells) {
        tile_ids[cell] = orb;
    }

    total_orbs = free_cells.size();
    orbs_collected = 0;

    for (int i = 0; i < tile_ids.size(); i++) {
        if (tile_ids[i] == marker)
            tile_ids[i] = empty;
    }

    free_cells.clear();

    for (int i = 0; i < tile_ids.size(); i++) {
        bool is_space = tile_ids[i] != wall;

        if (is_space)
            free_cells.push_back(i);
    }

    // Spawn agent
    Vector2 agent_pos{ static_cast<float>(agent_spawn_x) + 0.5f, static_cast<float>(main_height - 1 - agent_spawn_y) };

    // Spawn the player (agent)
    Entity agent = c.create_entity();

    c.add_component(agent, Component_Transform{ .position = agent_pos });
    c.add_component(agent, Component_Collision{ .bounds{ -0.5f, -0.5f, 1.0f, 1.0f } });
    c.add_component(agent, Component_Dynamics{});
    c.add_component(agent, Component_Agent{});
}

void System_Tilemap::render(int theme) {
    Rectangle camera_aabb{ (gr.camera_position.x - gr.camera_size.x * 0.5f / gr.camera_scale) * pixels_to_unit, (gr.camera_position.y - gr.camera_size.y * 0.5f / gr.camera_scale) * pixels_to_unit,
        gr.camera_size.x * pixels_to_unit / gr.camera_scale, gr.camera_size.y * pixels_to_unit / gr.camera_scale };

    int lower_x = std::floor(camera_aabb.x);
    int lower_y = std::floor(camera_aabb.y);
    int upper_x = std::ceil(camera_aabb.x + camera_aabb.width);
    int upper_y = std::ceil(camera_aabb.y + camera_aabb.height);
    
    for (int y = lower_y; y <= upper_y; y++)
        for (int x = lower_x; x <= upper_x; x++) {
            Tile_ID id = get(x, map_height - 1 - y);

            if (id == empty)
                continue;

            if (id >= id_to_textures.size() || id_to_textures[id].empty())
                continue;

            Asset_Texture* tex = &id_to_textures[id][theme];

            gr.render_texture(tex, (Vector2){ x * unit_to_pixels, y * unit_to_pixels }, unit_to_pixels / tex->width);
        }
}

std::pair<Vector2, bool> System_Tilemap::get_collision(Rectangle rectangle, const std::function<Collision_Type(Tile_ID)> &collision_id_func) {
    bool collided = false;

    int lower_x = std::floor(rectangle.x);
    int lower_y = std::floor(rectangle.y);
    int upper_x = std::ceil(rectangle.x + rectangle.width);
    int upper_y = std::ceil(rectangle.y + rectangle.height);

    Vector2 center{ rectangle.x + rectangle.width * 0.5f, rectangle.y + rectangle.height * 0.5f };

    Rectangle tile;
    tile.width = 1.0f;
    tile.height = 1.0f;
    
    for (int y = lower_y; y <= upper_y; y++)
        for (int x = lower_x; x <= upper_x; x++) {
            Tile_ID id = get(x, map_height - 1 - y);

            Collision_Type type = collision_id_func(id);

            if (type != none) {
                tile.x = x;
                tile.y = y;

                Rectangle collision = get_collision_overlap(rectangle, tile);

                if (collision.width != 0.0f || collision.height != 0.0f) {
                    Vector2 collision_center{ collision.x + collision.width * 0.5f, collision.y + collision.height * 0.5f };

                    if (collision.width > collision.height) {
                        rectangle.y = (collision_center.y > center.y ? tile.y - rectangle.height : tile.y + tile.height);
                        collided = true;
                    }
                }
            }
        }

    for (int y = lower_y; y <= upper_y; y++)
        for (int x = lower_x; x <= upper_x; x++) {
            Tile_ID id = get(x, map_height - 1 - y);

            Collision_Type type = collision_id_func(id);

            if (type != none) {
                tile.x = x;
                tile.y = y;

                Rectangle collision = get_collision_overlap(rectangle, tile);

                if (collision.width != 0.0f || collision.height != 0.0f) {
                    Vector2 collision_center{ collision.x + collision.width * 0.5f, collision.y + collision.height * 0.5f };

                    if (collision.width <= collision.height) {
                        rectangle.x = (collision_center.x > center.x ? tile.x - rectangle.width : tile.x + tile.width);
                        collided = true;
                    }
                }
            }
        }

    return std::make_pair(Vector2{ rectangle.x, rectangle.y }, collided);
}
