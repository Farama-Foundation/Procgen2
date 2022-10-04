#include "tilemap.h"
#include <iostream>

void System_Tilemap::init() {
    id_to_textures.resize(num_ids);

    // Load textures
    id_to_textures[wall_top].resize(wall_themes.size());
    id_to_textures[wall_mid].resize(wall_themes.size());

    for (int i = 0; i < wall_themes.size(); i++) {
        id_to_textures[wall_top][i].load("assets/kenney/Ground/" + wall_themes[i] + "/" + to_lower(wall_themes[i]) + "Mid.png");
        id_to_textures[wall_mid][i].load("assets/kenney/Ground/" + wall_themes[i] + "/" + to_lower(wall_themes[i]) + "Center.png");
    }

    id_to_textures[lava_top].resize(1);
    id_to_textures[lava_top][0].load("assets/kenney/Tiles/lavaTop_low.png");

    id_to_textures[lava_mid].resize(1);
    id_to_textures[lava_mid][0].load("assets/kenney/Tiles/lava.png");

    id_to_textures[crate].resize(crate_types.size());

    for (int i = 0; i < crate_types.size(); i++)
        id_to_textures[crate][i].load("assets/kenney/Tiles/" + crate_types[i] + ".png");

    // Preload enemies
    for (int i = 0; i < walking_enemies.size(); i++) {
        manager_texture.get("assets/kenney/Enemies/" + walking_enemies[i] + ".png");
        manager_texture.get("assets/kenney/Enemies/" + walking_enemies[i] + "_move.png");
    }

    manager_texture.get("assets/kenney/Enemies/sawHalf.png");
    manager_texture.get("assets/kenney/Enemies/sawHalf_move.png");
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
void System_Tilemap::spawn_enemy_saw(int x, int y) {
    Entity e = c.create_entity();

    Vector2 pos = { static_cast<float>(x) + 0.5f, static_cast<float>(map_height - 1 - y) + 0.5f };

    Component_Animation animation;
    animation.frames.resize(2);
    animation.frames[0] = manager_texture.get("assets/kenney/Enemies/sawHalf.png").texture;
    animation.frames[1] = manager_texture.get("assets/kenney/Enemies/sawHalf_move.png").texture;
    animation.rate = 1.0f / 60.0f; // Every frame at 60 fps

    c.add_component(e, Component_Transform{ .position{ pos } });
    c.add_component(e, Component_Sprite{ .position{ -0.5f, -0.5f }, .z = 1.0f });
    c.add_component(e, Component_Hazard{});
    c.add_component(e, animation);
}

void System_Tilemap::spawn_enemy_mob(int x, int y, std::mt19937 &rng) {
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    Entity e = c.create_entity();

    Vector2 pos = { static_cast<float>(x) + 0.5f, static_cast<float>(map_height - 1 - y) + 0.5f };

    std::uniform_int_distribution<int> walking_enemy_dist(0, walking_enemies.size() - 1);

    Texture2D tex = manager_texture.get("assets/kenney/Enemies/" + walking_enemies[walking_enemy_dist(rng)] + ".png").texture;

    c.add_component(e, Component_Transform{ .position{ pos } });
    c.add_component(e, Component_Sprite{ .position{ -0.5f, -0.5f }, .z = 1.0f, .texture = tex });
    c.add_component(e, Component_Hazard{});
    c.add_component(e, Component_Sweeper{ .velocity_x = 0.15f * ((dist01(rng) < 0.5f) * 2.0f - 1.0f) });
}

// Main map generation
void System_Tilemap::regenerate(std::mt19937 &rng, const Config &cfg) {
    const int main_width = 64;
    const int main_height = 64;
    const float max_jump = 1.5f;
    const float gravity = 0.2f;
    const float max_speed = 0.5f;

    this->map_width = main_width;
    this->map_height = main_height;

    tile_ids.resize(map_width * map_height);

    // Clear
    std::fill(tile_ids.begin(), tile_ids.end(), empty);

    // Initialize floors and walls
    set_area(0, 0, main_width, 1, wall_top);
    set_area(0, 0, 1, main_height, wall_mid);
    set_area(main_width - 1, 0, 1, main_height, wall_mid);
    set_area(0, main_height - 1, main_width, 1, wall_mid);

    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    std::uniform_int_distribution<int> difficulty_dist(1, 3);

    int difficulty = difficulty_dist(rng);

    std::uniform_int_distribution<int> section_dist(difficulty, 2 * difficulty - 1);

    int num_sections = section_dist(rng);

    int curr_x = 5;
    int curr_y = 1;

    int pit_thresh = difficulty;

    std::uniform_int_distribution<int> danger_dist(0, 2);

    int danger_type = danger_dist(rng);

    int w = main_width;

    float max_dxf = max_speed * 2.0f * max_jump / gravity;
    float max_dyf = max_jump * max_jump / (2.0f * gravity);

    // Cast to int
    int max_dx = max_dxf - 0.5f;
    int max_dy = max_dyf - 0.5f;

    bool allow_monsters = !cfg.easy_mode;

    for (int section = 0; section < num_sections; section++) {
        if (curr_x + 15 >= w)
            break;

        int difficult_offset = difficulty / 3;

        std::uniform_int_distribution<int> dy_dist(1 + difficult_offset, 4 + difficult_offset);

        int dy = (cfg.allow_dy ? dy_dist(rng) : 0);

        dy = std::min(dy, max_dy);

        // Flip
        if (curr_y >= 20 || (curr_y >= 5 && dist01(rng) < 0.5f))
            dy *= -1;

        std::uniform_int_distribution<int> dx_dist(3 + difficult_offset, 2 * difficulty + 2 + difficult_offset);

        int dx = dx_dist(rng);

        curr_y = std::max(1, curr_y + dy);

        std::uniform_int_distribution<int> pit_dist(0, 19);

        bool use_pit = cfg.allow_pit && (dx > 7) && (curr_y > 3) && (pit_dist(rng) >= pit_thresh);

        std::uniform_int_distribution<int> dist3(1, 3);

        if (use_pit) {
            int x1 = dist3(rng);
            int x2 = dist3(rng);
            int pit_width = dx - x1 - x2;

            if (pit_width > max_dx) {
                pit_width = max_dx;
                x2 = dx - x1 - pit_width;
            }

            set_area_with_top(curr_x, 0, x1, curr_y, wall_mid, wall_top);
            set_area_with_top(curr_x + dx - x2, 0, x2, curr_y, wall_mid, wall_top);

            std::uniform_int_distribution<int> lava_height_dist(1, curr_y - 3);

            int lava_height = lava_height_dist(rng);

            switch (danger_type) {
            case 0:
                set_area_with_top(curr_x + x1, 1, pit_width, lava_height, lava_mid, lava_top);

                break;
            case 1:
                for (int i = 0; i < pit_width; i++)
                    spawn_enemy_saw(curr_x + x1 + i, 1);

                break;
            case 2:
                for (int i = 0; i < pit_width; i++)
                    spawn_enemy_mob(curr_x + x1 + i, 1, rng);

                break;
            }

            if (pit_width > 4) {
                std::uniform_int_distribution<int> dist2(1, 2);

                int x3, w1;

                if (pit_width == 5) {
                    x3 = dist2(rng);
                    w1 = dist2(rng);
                }
                else if (pit_width == 6) {
                    x3 = dist2(rng) + 1;
                    w1 = dist2(rng);
                }
                else {
                    x3 = dist2(rng) + 1;
                    int x4 = dist2(rng) + 1;
                    w1 = pit_width - x3 - x4;
                }

                set_area_with_top(curr_x + x1 + x3, curr_y - 1, w1, 1, wall_mid, wall_top);
            }
        }
        else {
            set_area_with_top(curr_x, 0, dx, curr_y, wall_mid, wall_top);

            int ob1_x = -1;
            int ob2_x = -1;

            std::uniform_int_distribution<int> spawn_dist(0, 9);

            if (spawn_dist(rng) < (2 * difficulty) && dx > 3) {
                std::uniform_int_distribution<int> x_dist(1, dx - 2);

                ob1_x = curr_x + x_dist(rng);

                spawn_enemy_saw(ob1_x, curr_y);
            }

            if (cfg.allow_mobs && spawn_dist(rng) < difficulty && dx > 3 && max_dx >= 4) {
                std::uniform_int_distribution<int> x_dist(1, dx - 2);

                ob1_x = curr_x + x_dist(rng);

                spawn_enemy_mob(ob1_x, curr_y, rng);
            }

             if (cfg.allow_crate) {
                for (int i = 0; i < 2; i++) {
                    std::uniform_int_distribution<int> x_dist(1, dx - 2);

                    int crate_x = curr_x + x_dist(rng);

                    if (dist01(rng) < 0.5f && ob1_x != crate_x && ob2_x != crate_x) {
                        int pile_height = dist3(rng);

                        for (int j = 0; j < pile_height; j++)
                            set(crate_x, curr_y + j, crate);
                    }
                }
            }
        }

        curr_x += dx;
    }

    set_area_with_top(curr_x, 0, 1, curr_y, wall_mid, wall_top);

    set_area(curr_x + 1, 0, main_width - curr_x, main_height, wall_mid);
}

void System_Tilemap::render(const Rectangle &camera_aabb, int theme) {
    int lower_x = std::floor(camera_aabb.x);
    int lower_y = std::floor(camera_aabb.y);
    int upper_x = std::ceil(camera_aabb.x + camera_aabb.width);
    int upper_y = std::ceil(camera_aabb.y + camera_aabb.height);
    
    // Temporary rng with constant seed to render visual-only randomness
    std::mt19937 render_rng(0);

    std::uniform_int_distribution<int> crate_dist(0, crate_types.size() - 1);

    for (int y = lower_y; y <= upper_y; y++)
        for (int x = lower_x; x <= upper_x; x++) {
            Tile_ID id = get(x, map_height - 1 - y);

            if (id == 0) // Empty
                continue;

            Texture2D tex;

            if (id == wall_mid || id == wall_top)
                tex = id_to_textures[id][theme].texture;
            else if (id == lava_mid || id == lava_top)
                tex = id_to_textures[id][0].texture;
            else if (id == crate)
                tex = id_to_textures[id][crate_dist(render_rng)].texture;

            DrawTextureEx(tex, (Vector2){ x * unit_to_pixels, y * unit_to_pixels }, 0.0f, unit_to_pixels / tex.width, WHITE);
        }
}
