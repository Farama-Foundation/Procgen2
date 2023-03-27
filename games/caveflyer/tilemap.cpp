#include "tilemap.h"

#include "maze_generator.h"
#include "room_generator.h"

void System_Tilemap::init() {
    id_to_textures.resize(num_ids);

    // Load textures
    id_to_textures[wall].resize(1);

    id_to_textures[wall][0].load("assets/misc_assets/groundA.png");

    // Pre-load some assets
    manager_texture.get("assets/misc_assets/ufoGreen2.png");
    manager_texture.get("assets/misc_assets/ufoRed2.png");
    manager_texture.get("assets/misc_assets/meteorBrown_big1.png");
    manager_texture.get("assets/misc_assets/enemyShipBlue4.png");
    manager_texture.get("assets/misc_assets/laserBlue02.png");
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
void System_Tilemap::spawn_obstacle(int cell) {
    int x = cell / map_height;
    int y = cell % map_height;

    Entity e = c.create_entity();

    Vector2 pos = { static_cast<float>(x) + 0.5f, static_cast<float>(map_height - 1 - y) + 0.5f };

    Asset_Texture* texture = &manager_texture.get("assets/misc_assets/meteorBrown_big1.png");

    c.add_component(e, Component_Transform{ .position{ pos } });
    c.add_component(e, Component_Sprite{ .position{ -0.4f, -0.4f }, .scale=0.8f, .z = 1.0f, .texture = texture });
    c.add_component(e, Component_Hazard{ .destroyable = false });
    c.add_component(e, Component_Collision{ .bounds{ -0.25f, -0.25f, 0.5f, 0.5f }});
}

void System_Tilemap::spawn_target(int cell) {
    int x = cell / map_height;
    int y = cell % map_height;

    Entity e = c.create_entity();

    Vector2 pos = { static_cast<float>(x) + 0.5f, static_cast<float>(map_height - 1 - y) + 0.5f };

    Asset_Texture* texture = &manager_texture.get("assets/misc_assets/ufoRed2.png");

    c.add_component(e, Component_Transform{ .position{ pos } });
    c.add_component(e, Component_Sprite{ .position{ -0.4f, -0.4f }, .scale=0.8f, .z = 1.0f, .texture = texture });
    c.add_component(e, Component_Hazard{ .destroyable = true });
    c.add_component(e, Component_Collision{ .bounds{ -0.25f, -0.25f, 0.5f, 0.5f }});
}

void System_Tilemap::spawn_enemy(int cell, const Vector2 &agent_pos, std::mt19937 &rng) {
    int x = cell / map_height;
    int y = cell % map_height;

    Entity e = c.create_entity();

    Vector2 pos = { static_cast<float>(x) + 0.5f, static_cast<float>(map_height - 1 - y) + 0.5f };

    Asset_Texture* texture = &manager_texture.get("assets/misc_assets/enemyShipBlue4.png");

    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    float vel_component = (0.1f * dist01(rng) + 0.1f) * (dist01(rng) < 0.5f ? 1.0f : -1.0f);

    int collision = check_neighbors(pos, agent_pos);

    Vector2 vel{ 0.0f, 0.0f };

    if (collision == 0) {
        if (dist01(rng) < 0.5f)
            vel.x = vel_component;
        else
            vel.y = vel_component;
    } 
    else if (collision == 1)
        vel.x = vel_component;
    else
        vel.y = vel_component;

    c.add_component(e, Component_Transform{ .position{ pos } });
    c.add_component(e, Component_Dynamics{ .velocity{ vel } });
    c.add_component(e, Component_Sprite{ .position{ -0.4f, -0.4f }, .scale=0.8f, .z = 1.0f, .texture = texture });
    c.add_component(e, Component_Hazard{ .destroyable = false });
    c.add_component(e, Component_Collision{ .bounds{ -0.4f, -0.4f, 0.8f, 0.8f }});
    c.add_component(e, Component_Mob_AI{});
}

int System_Tilemap::check_neighbors(const Vector2 &p0, const Vector2 &p1) {
    const float neighborhood = 2.0f;
    const float epsilon = 0.001f;

    if (std::abs(p0.x - p1.x) <= epsilon && std::abs(p0.y - p1.y) <= neighborhood)
        return 1; // Collision if entities move in the Y-axis

    if (std::abs(p0.x - p1.x) <= neighborhood && std::abs(p0.y - p1.y) <= epsilon)
        return 2; // Collision if entities move in the X-axis

    return 0; // No Collision
}

// Main map generation
void System_Tilemap::regenerate(std::mt19937 &rng, const Config &cfg) {
    int world_dim;

    if (cfg.mode == hard_mode)
        world_dim = 40;
    else if (cfg.mode == memory_mode)
        world_dim = 45;
    else
        world_dim = 20;

    const int main_width = world_dim;
    const int main_height = world_dim;

    this->map_width = main_width;
    this->map_height = main_height;

    tile_ids.resize(map_width * map_height);

    // Random seed state for room generator
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    Room_Generator room_generator;
    room_generator.init(main_width, main_height);

    for (int i = 0; i < tile_ids.size(); i++)
        room_generator.grid[i] = dist01(rng) < 0.5f ? 1 : 0;

    for (int it = 0; it < 2; it++)
        room_generator.update();

    std::unordered_set<int> best_room;
    room_generator.find_best_room(best_room);
    assert(!best_room.empty());

    // Copy to actual tiles
    for (int i = 0; i < tile_ids.size(); i++)
        tile_ids[i] = room_generator.grid[i] == 1 ? wall : empty;

    std::vector<int> free_cells;

    for (int i : best_room) {
        tile_ids[i] = empty;
        free_cells.push_back(i);
    }

    std::uniform_int_distribution<int> free_cell_dist(0, free_cells.size() - 1);

    int goal_index = free_cell_dist(rng);
    int agent_index = free_cell_dist(rng);

    if (agent_index == goal_index)
        agent_index = (agent_index + 1) % free_cells.size();

    int goal_cell = free_cells[goal_index];
    int agent_cell = free_cells[agent_index];

    // Spawn the goal
    int goal_x = goal_cell / main_height;
    int goal_y = goal_cell % main_height;

    Entity goal = c.create_entity();

    Vector2 goal_pos = { static_cast<float>(goal_x) + 0.5f, static_cast<float>(map_height - 1 - goal_y) + 0.5f };

    c.add_component(goal, Component_Transform{ .position{ goal_pos } });
    c.add_component(goal, Component_Sprite{ .position{ -0.4f, -0.4f }, .scale=0.8f, .z = 1.0f, .texture = &manager_texture.get("assets/misc_assets/ufoGreen2.png") });
    c.add_component(goal, Component_Goal{});
    c.add_component(goal, Component_Collision{ .bounds{ -0.4f, -0.4f, 0.8f, 0.8f }});

    info.goal_pos = goal_pos;

    Vector2 agent_pos{ static_cast<float>(static_cast<int>(agent_cell / main_height)) + 0.5f, static_cast<float>(main_height - 1 - (agent_cell % main_height)) };

    // Spawn the player (agent)
    Entity agent = c.create_entity();

    c.add_component(agent, Component_Transform{ .position = agent_pos });
    c.add_component(agent, Component_Collision{ .bounds{ -0.4f, -0.4f, 0.8f, 0.8f } });
    c.add_component(agent, Component_Dynamics{});
    c.add_component(agent, Component_Agent{});
    c.add_component(agent, Component_Particles{ .particles = std::vector<Particle>(10), .offset{ 0.0f, 0.3f } });

    std::vector<int> goal_path;
    room_generator.find_path(agent_cell, goal_cell, goal_path);

    bool should_prune = cfg.mode != memory_mode;

    if (should_prune) {
        std::unordered_set<int> wide_path;
        wide_path.insert(goal_path.begin(), goal_path.end());
        room_generator.expand_room(wide_path, 4);

        for (int i = 0; i < tile_ids.size(); i++)
            tile_ids[i] = wall;

        for (int i : wide_path)
            tile_ids[i] = empty;
    }

    for (int iteration = 0; iteration < 4; iteration++) {
        room_generator.update();

        for (int i : goal_path)
            tile_ids[i] = empty;
    }

    for (int i : goal_path)
        tile_ids[i] = marker;

    free_cells.clear();

    for (int i = 0; i < tile_ids.size(); i++) {
        if (tile_ids[i] == empty)
            free_cells.push_back(i);
    }

    int chunk_size = static_cast<int>(free_cells.size()) / 80;
    int num_objects = 3 * chunk_size;

    std::vector<int> obstacle_indices(num_objects);

    for (int i = 0; i < num_objects; i++) {
        std::uniform_int_distribution<int> free_cell_dist(0, free_cells.size() - 1);

        int index = free_cell_dist(rng);

        bool repeat_found;

        do {
            repeat_found = false;

            // See if already have this index
            for (int j = 0; j < i; j++) {
                if (obstacle_indices[j] == index) {
                    index = (index + 1) % free_cells.size();

                    repeat_found = true;

                    break;
                }
            }
        }
        while (repeat_found);

        obstacle_indices[i] = index;

        int cell = free_cells[index];

        if (i < chunk_size)
            spawn_obstacle(cell);
        else if (i < 2 * chunk_size)
            spawn_target(cell);
        else
            spawn_enemy(cell, agent_pos, rng);
    }

    for (int i = 0; i < tile_ids.size(); i++) {
        if (tile_ids[i] == marker)
            tile_ids[i] = empty;
    }
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
