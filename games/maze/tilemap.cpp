#include "tilemap.h"

#include "maze_generator.h"
#include <random>
//tmp include
#include <chrono>
#include <iostream>

const int GOAL = 2;

void System_Tilemap::init() {
    id_to_textures.resize(num_ids);

    // Load textures
    id_to_textures[wall].load("assets/kenney/Ground/Sand/sandCenter.png");

    // Pre-load
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

// Main map generation
int num_mazegens = 0;
double total_gentime = 0;
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

    tile_ids.resize(map_width * map_height);

    // Clear
    std::fill(tile_ids.begin(), tile_ids.end(), wall);

    std::uniform_int_distribution<int> n_dist(0, (world_dim - 1) / 2 - 1);
    const int maze_dim = n_dist(rng)*2 + 3;
    int margin = (world_dim - maze_dim) / 2;

    // Generate maze
    Maze_Generator maze_generator;
    maze_generator.generate_maze(maze_dim, maze_dim, rng);
    // Place goal (cheese)
    maze_generator.place_object(GOAL, rng);

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
        }
    }

    // Spawn the goal
    Entity goal = c.create_entity();

    Vector2 pos = { static_cast<float>(goal_x) + 0.5f, static_cast<float>(map_height - 1 - goal_y) + 0.5f };

    c.add_component(goal, Component_Transform{ .position{ pos } });
    c.add_component(goal, Component_Sprite{ .position{ -0.48f, -0.5f }, .scale = 0.95f, .z = 1.0f, .texture = &manager_texture.get("assets/misc_assets/cheese.png") });
    c.add_component(goal, Component_Goal{});
    c.add_component(goal, Component_Collision{ .bounds{ -0.5f, -0.5f, 1.0f, 1.0f }});

    agent_start_x = margin;
    agent_start_y = margin;
    Vector2 agent_pos{ static_cast<float>(agent_start_x) + 0.5f, static_cast<float>(main_height - 1 - agent_start_y) + 0.5f };

    // Spawn the player (agent)
    Entity agent = c.create_entity();

    c.add_component(agent, Component_Transform{ .position = agent_pos });
    c.add_component(agent, Component_Collision{ .bounds{ -0.5f, -0.5f, 1.0f, 1.0f } });
    c.add_component(agent, Component_Agent{});
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

            Asset_Texture* tex = &id_to_textures[id];

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
