#include "tilemap.h"

#include "maze_generator.h"
#include <random>

const int GOAL = 2;

void System_Tilemap::init() {
    id_to_textures.resize(num_ids);

    // Load textures
    id_to_textures[wall].load("assets/kenney/Ground/Sand/sandCenter.png");

    // Pre-load
    // manager_texture.get("assets/misc_assets/spikeMan_stand.png");
    manager_texture.get("assets/misc_assets/cheese.png");
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

// Spawning helpers
// void System_Tilemap::spawn_spike(int x, int y) {
//     Entity e = c.create_entity();

//     Vector2 pos = { static_cast<float>(x) + 0.5f, static_cast<float>(map_height - 1 - y) + 0.5f };

//     Asset_Texture* texture = &manager_texture.get("assets/misc_assets/spikeMan_stand.png");

//     c.add_component(e, Component_Transform{ .position{ pos } });
//     c.add_component(e, Component_Sprite{ .position{ -0.25f, -0.25f }, .scale=0.4f, .z = 1.0f, .texture = texture });
//     c.add_component(e, Component_Hazard{});
//     c.add_component(e, Component_Collision{ .bounds{ -0.25f, -0.25f, 0.5f, 0.5f }});
// }

// bool System_Tilemap::is_space_on_ground(int x, int y) {
//     if (get(x, y) != empty)
//         return false;

//     if (get(x, y + 1) != empty)
//         return false;

//     int below_id = get(x, y - 1);

//     return below_id == wall || below_id == out_of_bounds;
// }

// bool System_Tilemap::is_top_wall(int x, int y) {
//     return get(x, y) == wall && get(x, y + 1) == empty;
// }

// bool System_Tilemap::is_left_wall(int x, int y) {
//     return get(x, y) == wall && get(x + 1, y) == empty;
// }

// bool System_Tilemap::is_right_wall(int x, int y) {
//     return get(x, y) == wall && get(x - 1, y) == empty;
// }

// Main map generation
void System_Tilemap::regenerate(std::mt19937 &rng, const Config &cfg) {
    int world_dim;

    if (cfg.mode == hard_mode)
        world_dim = 25;
    else if (cfg.mode == memory_mode)
        world_dim = 31;
    else
        world_dim = 15;

    const int main_width = world_dim;
    const int main_height = world_dim;

    this->map_width = main_width;
    this->map_height = main_height;

    // gr.camera_position.x = 0.5*main_width;
    // gr.camera_position.y = 0.5*main_height;
    // gr.camera_position.x = gr.camera_position.y = 0;

    tile_ids.resize(map_width * map_height);
    // crate_type_indices.resize(tile_ids.size());

    // Clear
    std::fill(tile_ids.begin(), tile_ids.end(), wall);

    std::uniform_int_distribution<int> n_dist(0, (world_dim - 1) / 2 - 1);
    // const int maze_scale = 3;
    const int maze_dim = n_dist(rng)*2 + 3;
    int margin = (world_dim - maze_dim) / 2;

    // Generate maze with no dead ends
    Maze_Generator maze_generator;
    maze_generator.generate_maze(maze_dim, maze_dim, rng);
    maze_generator.place_object(GOAL, rng);
    // place goal

    // Room_Generator room_generator;
    // room_generator.init(main_width, main_height);

    // std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    // for (int i = 0; i < tile_ids.size(); i++) {
        // int obj = maze_generator.grid[maze_generator.get_index((i / main_height) / maze_scale + 1, (i % main_height) / maze_scale + 1)];

        // float prob = obj == 1 ? 0.8f : 0.2f; // If is wall, higher probability

        // tile_ids[i] = dist01(rng) < prob ? wall : empty; // Wall or space

        // room_generator.grid[i] = (tile_ids[i] == wall ? 1 : 0);
    // }
    int goal_x = 0;
    int goal_y = 0;

    int agent_start_x = margin;
    int agent_start_y = margin;

    for (int i = 0; i < maze_dim; ++i) {
        for (int j = 0; j < maze_dim; ++j) {
            int curr_tile_type = maze_generator.get(i + maze_offset, j + maze_offset);
            set(i + margin, j + margin, curr_tile_type == WALL_CELL ? wall : empty);
            if (curr_tile_type == GOAL) {
                goal_x = i + margin;
                goal_y = j + margin;
            }
            // else if (curr_tile_type == START_CELL){
            //     agent_start_x = i + margin;
            //     agent_start_y = j + margin;
            // }
        }
    }

    // for (int it = 0; it < 2; it++)
    //     room_generator.update();

    // Add border cells. needed for helping with solvability and proper rendering of bottommost floor tiles
    // for (int i = 0; i < main_width; i++) {
    //     set(i, 0, wall);
    //     set(i, main_height - 1, wall);

    //     room_generator.set(i, 0, 1);
    //     room_generator.set(i, main_height - 1, 1);
    // }

    // for (int i = 0; i < main_height; i++) {
    //     set(0, i, wall);
    //     set(main_width - 1, i, wall);

    //     room_generator.set(0, i, 1);
    //     room_generator.set(main_width - 1, i, 1);
    // }

    // std::unordered_set<int> best_room;
    // room_generator.find_best_room(best_room);

    // for (int i = 0; i < tile_ids.size(); i++)
    //     tile_ids[i] = wall;

    // std::vector<int> free_cells;

    // for (int i : best_room) {
    //     tile_ids[i] = empty;
    //     free_cells.push_back(i);
    // }

    // std::uniform_int_distribution<int> free_cell_dist(0, free_cells.size() - 1);

    // int goal_cell = free_cells[free_cell_dist(rng)];

    // std::vector<int> agent_candidates;

    // for (int x = 0; x < main_width; x++)
    //     for (int y = 0; y < main_height; y++) {
    //         int i = y + main_height * x;

    //         if (is_space_on_ground(x, y) && i != goal_cell)
    //             agent_candidates.push_back(i);
    //     }

    // std::uniform_int_distribution<int> agent_cell_dist(0, agent_candidates.size() - 1);

    // int agent_cell = agent_candidates[agent_cell_dist(rng)];

    // std::vector<int> goal_path;
    // room_generator.find_path(agent_cell, goal_cell, goal_path);

    // bool should_prune = cfg.mode != memory_mode;

    // if (should_prune) {
    //     std::unordered_set<int> wide_path;
    //     wide_path.insert(goal_path.begin(), goal_path.end());
    //     room_generator.expand_room(wide_path, 4);

    //     for (int i = 0; i < tile_ids.size(); i++)
    //         tile_ids[i] = wall;

    //     for (int i : wide_path)
    //         tile_ids[i] = empty;
    // }

    // Spawn the goal
    Entity goal = c.create_entity();

    Vector2 pos = { static_cast<float>(goal_x) + 0.5f, static_cast<float>(map_height - 1 - goal_y) + 0.5f };
    // Vector2 pos = { static_cast<float>(goal_x) + 0.5f, (static_cast<float>(goal_y) + 0.5f) };

    c.add_component(goal, Component_Transform{ .position{ pos } });
    c.add_component(goal, Component_Sprite{ .position{ -0.48f, -0.5f }, .scale = 0.95f, .z = 1.0f, .texture = &manager_texture.get("assets/misc_assets/cheese.png") });
    c.add_component(goal, Component_Goal{});
    c.add_component(goal, Component_Collision{ .bounds{ -0.5f, -0.5f, 1.0f, 1.0f }});

    // float spike_prob = cfg.mode == memory_mode ? 0.0f : 0.2f;

    // for (int x = 0; x < main_width; x++)
    //     for (int y = 0; y < main_height; y++) {
    //         if (is_space_on_ground(x, y) && is_space_on_ground(x - 1, y) && is_space_on_ground(x + 1, y)) {
    //             if (dist01(rng) < spike_prob)
    //                 set(x, y, spike);
    //         }
    //     }

    // std::uniform_int_distribution<int> dist3(0, 2);

    // // We prevent log vertical walls to improve solvability
    // for (int x = 0; x < main_width; x++)
    //     for (int y = 0; y < main_height; y++) {
    //         if (is_left_wall(x, y) && is_left_wall(x, y + 1) && is_left_wall(x, y + 2))
    //             set(x, y + dist3(rng), empty);

    //         if (is_right_wall(x, y) && is_right_wall(x, y + 1) && is_right_wall(x, y + 2))
    //             set(x, y + dist3(rng), empty);
    //     }

    // Vector2 agent_pos{ static_cast<float>(static_cast<int>(agent_start_x)) + 0.5f, static_cast<float>(main_height - 1 - agent_start_y) };
    agent_start_x = margin;
    agent_start_y = margin;
    Vector2 agent_pos{ static_cast<float>(agent_start_x) + 0.5f, static_cast<float>(main_height - 1 - agent_start_y) + 0.5f };

    // Spawn the player (agent)
    Entity agent = c.create_entity();

    c.add_component(agent, Component_Transform{ .position = agent_pos });
    c.add_component(agent, Component_Collision{ .bounds{ -0.5f, -0.5f, 1.0f, 1.0f } });
    // c.add_component(agent, Component_Dynamics{});
    c.add_component(agent, Component_Agent{});

    // std::vector<int> spike_cells;

    // for (int i = 0; i < tile_ids.size(); i++) {
    //     if (tile_ids[i] == spike) {
    //         tile_ids[i] = empty;

    //         if (i != agent_cell && i != goal_cell) // Avoid placing spike in agent or goal position
    //             spawn_spike(i / main_height, i % main_height);
    //     }
    // }

    // // Set tops
    // for (int x = 0; x < main_width; x++)
    //     for (int y = 0; y < main_height; y++) {
    //         if (is_top_wall(x, y))
    //             set(x, y, wall_top);
    //     }
}

void System_Tilemap::render() {
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

            // if (id >= id_to_textures.size() || id_to_textures[id].empty())
            //     continue;

            Asset_Texture* tex = &id_to_textures[id];

            // gr.render_texture(tex, (Vector2){ (x - 0.5f*map_width) * unit_to_pixels, (y - 0.5f*map_height) * unit_to_pixels }, unit_to_pixels / tex->width);
            gr.render_texture(tex, (Vector2){ x * unit_to_pixels, y * unit_to_pixels }, unit_to_pixels / tex->width);
        }
}

std::pair<Vector2, bool> System_Tilemap::get_collision(Rectangle rectangle, const std::function<Collision_Type(Tile_ID)> &collision_id_func, bool fallthrough, float step_y) {
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
