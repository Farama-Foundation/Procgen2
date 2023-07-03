#include "tilemap.h"

void System_Tilemap::init() {
    id_to_textures.resize(num_ids);

    // Load textures
    id_to_textures[wall_top].resize(4);
    id_to_textures[wall_mid].resize(4);

    id_to_textures[wall_top][0].load("assets/platformer/tileBlue_05.png");
    id_to_textures[wall_top][1].load("assets/platformer/tileGreen_05.png");
    id_to_textures[wall_top][2].load("assets/platformer/tileYellow_06.png");
    id_to_textures[wall_top][3].load("assets/platformer/tileBrown_06.png");

    id_to_textures[wall_mid][0].load("assets/platformer/tileBlue_08.png");
    id_to_textures[wall_mid][1].load("assets/platformer/tileGreen_08.png");
    id_to_textures[wall_mid][2].load("assets/platformer/tileYellow_09.png");
    id_to_textures[wall_mid][3].load("assets/platformer/tileBrown_09.png");

    // Preload enemies
    manager_texture.get("assets/platformer/enemySwimming_1.png");
    manager_texture.get("assets/platformer/enemySwimming_2.png");

    // Pre-load coin
    manager_texture.get("assets/misc_assets/yellowCrystal.png");
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

void System_Tilemap::spawn_enemy_mob(int x, int y, std::mt19937 &rng) {
    std::uniform_int_distribution<int> dist2(0, 1);

    Entity e = c.create_entity();
    
    Vector2 pos = { static_cast<float>(x) + 0.5f, static_cast<float>(map_height - 1 - y) + 0.5f };

    Component_Animation animation;
    animation.frames.resize(2);
    animation.frames[0] = &manager_texture.get("assets/platformer/enemySwimming_1.png");
    animation.frames[1] = &manager_texture.get("assets/platformer/enemySwimming_2.png");
    animation.rate = 0.2f;

    c.add_component(e, Component_Transform{ .position{ pos } });
    c.add_component(e, Component_Sprite{ .position{ -0.4f, -0.4f }, .scale=1.0f, .z = 1.0f });
    c.add_component(e, Component_Collision{ .bounds{ -0.4f, -0.4f, 0.8f, 0.8f }});
    c.add_component(e, Component_Mob_AI{ .velocity_x = 0.15f * (dist2(rng) * 2.0f - 1.0f), .spawn_x = x});
    c.add_component(e, animation);
}

void System_Tilemap::spawn_point(int x, int y) {

    Entity e = c.create_entity();

    Vector2 pos = { static_cast<float>(x) + 0.5f, static_cast<float>(map_height - 1 - y) + 0.5f };
    Asset_Texture* texture = &manager_texture.get("assets/misc_assets/yellowCrystal.png");

    c.add_component(e, Component_Transform{ .position{ pos } });
    c.add_component(e, Component_Sprite{ .position{ -0.5f, -0.5f }, .z = 1.0f, .texture = texture });
    c.add_component(e, Component_Collision{ .bounds{ -0.5f, -0.5f, 1.0f, 1.0f }});
    c.add_component(e, Component_Point{});
}


// Main map generation
void System_Tilemap::regenerate(std::mt19937 &rng, const Config &cfg) {
    const int main_width = 20;
    const int main_height = 64;
    const float max_jump = 1.5f;
    const float gravity = 0.2f;

    this->map_width = main_width;
    this->map_height = main_height;

    tile_ids.resize(map_width * map_height);

    // Clear
    std::fill(tile_ids.begin(), tile_ids.end(), empty);

    // Initialize floors and walls
    set_area_with_top(0, 0, main_width, 1, wall_mid, wall_top);
    set_area(0, 0, 1, main_height, wall_mid);
    set_area(main_width - 1, 0, 1, main_height, wall_mid);
    set_area(0, main_height - 1, main_width, 1, wall_mid);

    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    std::uniform_int_distribution<int> dist2(0, 1);
    
    std::uniform_int_distribution<int> difficulty_dist(1, 3);

    int difficulty = difficulty_dist(rng);

    int min_platforms = difficulty * difficulty + 1;
    int max_platforms = (difficulty + 1) * (difficulty + 1) + 1;

    std::uniform_int_distribution<int> platforms_dist(min_platforms, max_platforms);

    int num_platforms = platforms_dist(rng);

    // total_points = 0;

    std::uniform_int_distribution<int> init_x_dist(2, main_width - 3);

    int curr_x = init_x_dist(rng);
    int curr_y = 1;

    int margin_x = 3;
    float enemy_prob = cfg.easy_mode ? .2 : .5;

    float max_dyf = max_jump * max_jump / (2.0f * gravity);

    // Cast to int
    int max_dy = max_dyf - 0.5f;

    std::uniform_int_distribution<int> init_y_dist(3, max_dy - 1);

    for (int platform = 0; platform < num_platforms; platform++) {

        int delta_y = init_y_dist(rng);

        // Only spawn enemies that won't be trapped in tight spaces
        bool can_spawn_enemy = (curr_x >= margin_x) && (curr_x <= main_width - 1 - margin_x);

        if (can_spawn_enemy && (dist01(rng) < enemy_prob))
            spawn_enemy_mob(curr_x, curr_y + dist2(rng) + 2, rng);

        curr_y += delta_y;

        std::uniform_int_distribution<int> dist_platform_len(0, 9);
        int plat_len = 2 + dist_platform_len(rng);

        // Direction of platform creation
        int vx = dist2(rng) * 2 - 1;
        if (curr_x < margin_x)
            vx = 1;
        if (curr_x > main_width - margin_x)
            vx = -1;

        std::vector<int> candidates;

        // Init point positions
        for (int j = 0; j < plat_len; j++) {
            int nx = curr_x + (j + 1) * vx;
            if (nx <= 0 || nx >= main_width - 1)
                break;
            candidates.push_back(nx);
            set_area_with_top(nx, curr_y, 1, 1, wall_mid, wall_top);
        }

        std::uniform_int_distribution<int> pos_dist(0, candidates.size() - 1);

        // Choose random point spawn
        if (dist01(rng) < .5 || platform == num_platforms - 1) {
            int point_x = candidates[pos_dist(rng)];
            spawn_point(point_x, curr_y + 1);
        }

        // Cycle from random x position 
        int next_x = candidates[pos_dist(rng)];
        curr_x = next_x;
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
