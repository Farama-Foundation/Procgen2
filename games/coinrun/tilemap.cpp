#include "tilemap.h"

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

    // Pre-load coin
    manager_texture.get("assets/kenney/Items/coinGold.png");
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
    animation.frames[0] = &manager_texture.get("assets/kenney/Enemies/sawHalf.png");
    animation.frames[1] = &manager_texture.get("assets/kenney/Enemies/sawHalf_move.png");
    animation.rate = 1.0f / 60.0f; // Every frame at 60 fps

    c.add_component(e, Component_Transform{ .position{ pos } });
    c.add_component(e, Component_Sprite{ .position{ -0.5f, -0.5f }, .z = 1.0f });
    c.add_component(e, Component_Hazard{});
    c.add_component(e, Component_Collision{ .bounds{ -0.5f, -0.5f, 1.0f, 1.0f }});
    c.add_component(e, animation);
}

void System_Tilemap::spawn_enemy_mob(int x, int y, std::mt19937 &rng) {
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    Entity e = c.create_entity();

    Vector2 pos = { static_cast<float>(x) + 0.5f, static_cast<float>(map_height - 1 - y) + 0.5f };

    std::uniform_int_distribution<int> walking_enemy_dist(0, walking_enemies.size() - 1);

    int enemy_index = walking_enemy_dist(rng);

    Component_Animation animation;
    animation.frames.resize(2);
    animation.frames[0] = &manager_texture.get("assets/kenney/Enemies/" + walking_enemies[enemy_index] + ".png");
    animation.frames[1] = &manager_texture.get("assets/kenney/Enemies/" + walking_enemies[enemy_index] + "_move.png");
    animation.rate = 0.5f;

    c.add_component(e, Component_Transform{ .position{ pos } });
    c.add_component(e, Component_Sprite{ .position{ -0.5f, -0.5f }, .z = 1.0f });
    c.add_component(e, Component_Hazard{});
    c.add_component(e, Component_Collision{ .bounds{ -0.5f, -0.48f, 1.0f, 0.98f }});
    c.add_component(e, Component_Mob_AI{ .velocity_x = 1.5f * ((dist01(rng) < 0.5f) * 2.0f - 1.0f) });
    c.add_component(e, Component_Particles{ .particles = std::vector<Particle>(10) });
    c.add_component(e, animation);
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
    crate_type_indices.resize(tile_ids.size());
    no_collide_mask.resize(tile_ids.size());

    // Clear
    std::fill(tile_ids.begin(), tile_ids.end(), empty);
    std::fill(no_collide_mask.begin(), no_collide_mask.end(), false);

    // Initialize floors and walls
    set_area(0, 0, main_width, 1, wall_top);
    set_area(0, 0, 1, main_height, wall_mid);
    set_area(main_width - 1, 0, 1, main_height, wall_mid);
    set_area(0, main_height - 1, main_width, 1, wall_mid);

    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    std::uniform_int_distribution<int> crate_dist(0, crate_types.size() - 1);

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

                        for (int j = 0; j < pile_height; j++) {
                            set(crate_x, curr_y + j, crate);
                            crate_type_indices[curr_y + j + crate_x * map_height] = crate_dist(rng);
                        }
                    }
                }
            }
        }

        curr_x += dx;
    }

    // Spawn the coin
    Entity coin = c.create_entity();

    Vector2 pos = { static_cast<float>(curr_x) + 0.5f, static_cast<float>(map_height - 1 - curr_y) + 0.5f };

    c.add_component(coin, Component_Transform{ .position{ pos } });
    c.add_component(coin, Component_Sprite{ .position{ -0.5f, -0.5f }, .z = 1.0f, .texture = &manager_texture.get("assets/kenney/Items/coinGold.png") });
    c.add_component(coin, Component_Goal{});
    c.add_component(coin, Component_Collision{ .bounds{ -0.5f, -0.5f, 1.0f, 1.0f }});

    set_area_with_top(curr_x, 0, 1, curr_y, wall_mid, wall_top);

    set_area(curr_x + 1, 0, main_width - curr_x, main_height, wall_mid);
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

            if (id == 0) // Empty
                continue;

            Asset_Texture* tex;

            if (id == wall_mid || id == wall_top)
                tex = &id_to_textures[id][theme];
            else if (id == lava_mid || id == lava_top)
                tex = &id_to_textures[id][0];
            else if (id == crate)
                tex = &id_to_textures[id][crate_type_indices[map_height - 1 - y + x * map_height]];

            gr.render_texture(tex, (Vector2){ x * unit_to_pixels, y * unit_to_pixels }, unit_to_pixels / tex->width);
        }
}

std::pair<Vector2, bool> System_Tilemap::get_collision(Rectangle rectangle, const std::function<Collision_Type(Tile_ID)> &collision_id_func, float velocity_y) {
    bool collided = false;

    int lower_x = std::floor(rectangle.x);
    int lower_y = std::floor(rectangle.y);
    int upper_x = std::ceil(rectangle.x + rectangle.width);
    int upper_y = std::ceil(rectangle.y + rectangle.height);

    Vector2 center{ rectangle.x + rectangle.width * 0.5f, rectangle.y + rectangle.height * 0.5f };

    Rectangle tile;
    tile.width = 1.0f;
    tile.height = 1.0f;
    
    // Need two passes to avoid "snagging" on tiles when sliding along a wall

    // Pass 1 (horizontal)
    for (int y = lower_y; y <= upper_y; y++)
        for (int x = lower_x; x <= upper_x; x++) {
            Tile_ID id = get(x, map_height - 1 - y);

            Collision_Type type = collision_id_func(id);

            if (type != none && !no_collide_mask[(map_height - 1 - y) + x * map_height]) {
                tile.x = x;
                tile.y = y;

                Rectangle collision = get_collision_overlap(rectangle, tile);

                if (collision.width != 0.0f || collision.height != 0.0f) {
                    collided = true;

                    Vector2 collision_center{ collision.x + collision.width * 0.5f, collision.y + collision.height * 0.5f };

                    if (collision.width <= collision.height) {
                        if (type != down_only)
                            rectangle.x = (collision_center.x > center.x ? tile.x - rectangle.width : tile.x + tile.width);
                    }
                }
            }
        }

    // Pass 2 (vertical)
    for (int y = lower_y; y <= upper_y; y++)
        for (int x = lower_x; x <= upper_x; x++) {
            Tile_ID id = get(x, map_height - 1 - y);

            Collision_Type type = collision_id_func(id);

            if (type != none && !no_collide_mask[(map_height - 1 - y) + x * map_height]) {
                tile.x = x;
                tile.y = y;

                Rectangle collision = get_collision_overlap(rectangle, tile);

                if (collision.width != 0.0f || collision.height != 0.0f) {
                    Vector2 collision_center{ collision.x + collision.width * 0.5f, collision.y + collision.height * 0.5f };

                    if (collision.width > collision.height) {
                        if (type == down_only)
                            rectangle.y = (velocity_y > 0.0f ? (collision_center.y > center.y ? tile.y - rectangle.height : tile.y + tile.height) : rectangle.y);
                        else
                            rectangle.y = (collision_center.y > center.y ? tile.y - rectangle.height : tile.y + tile.height);
                    }
                }
            }
        }

    return std::make_pair(Vector2{ rectangle.x, rectangle.y }, collided);
}

void System_Tilemap::update_no_collide(const Rectangle &player_rectangle, const Rectangle &outer_rectangle) {
    Rectangle shifted_rectangle = player_rectangle;
    shifted_rectangle.y -= 0.5f;

    // Only check "real" tiles (not out of bounds) by clamping to 0, width/height range
    int lower_x = std::max(0, static_cast<int>(std::floor(outer_rectangle.x)));
    int lower_y = std::max(0, static_cast<int>(std::floor(outer_rectangle.y)));
    int upper_x = std::min(map_width - 1, static_cast<int>(std::ceil(outer_rectangle.x + outer_rectangle.width)));
    int upper_y = std::min(map_height - 1, static_cast<int>(std::ceil(outer_rectangle.y + outer_rectangle.height)));

    Rectangle tile;
    tile.width = 1.0f;
    tile.height = 1.0f;
    
    for (int y = lower_y; y <= upper_y; y++)
        for (int x = lower_x; x <= upper_x; x++) {
            Tile_ID id = get(x, map_height - 1 - y);

            int index = (map_height - 1 - y) + x * map_height;

            if (id == crate) {
                tile.x = x;
                tile.y = y;

                if (!check_collision(player_rectangle, tile))
                    no_collide_mask[index] = false;
                else if (check_collision(shifted_rectangle, tile))
                    no_collide_mask[index] = true;
            }
        }
}
