#include "tilemap.h"

void System_Tilemap::init() {
    id_to_textures.resize(num_ids);

    id_to_textures[wall_mid].resize(wall_themes.size());
    for (int i = 0; i < (int)wall_themes.size(); i++)
        id_to_textures[wall_mid][i].load(wall_themes[i]);

    id_to_textures[fire_tile].resize(1);
    id_to_textures[fire_tile][0].load("assets/misc_assets/bomb.png");

    id_to_textures[bomb_tile].resize(1);
    id_to_textures[bomb_tile][0].load("assets/misc_assets/bomb.png");

    manager_texture.get("assets/platformer/shroom1.png");
    manager_texture.get("assets/platformer/shroom2.png");
    manager_texture.get("assets/platformer/shroom3.png");
    manager_texture.get("assets/platformer/shroom4.png");
    manager_texture.get("assets/platformer/shroom5.png");
    manager_texture.get("assets/platformer/shroom6.png");
    manager_texture.get("assets/misc_assets/saw.png");
    manager_texture.get("assets/misc_assets/explosion1.png");
}

void System_Tilemap::set_area(int x, int y, int width, int height, Tile_ID id) {
    for (int dx = 0; dx < width; dx++)
        for (int dy = 0; dy < height; dy++)
            set(x + dx, y + dy, id);
}

void System_Tilemap::regenerate(std::mt19937 &rng, const Config &cfg) {
    const int main_width = 64;
    const int main_height = 64;
    const float max_jump = cfg.easy_mode ? 1.25f : 1.5f;
    const float gravity = 0.2f;

    this->map_width = main_width;
    this->map_height = main_height;

    tile_ids.assign(map_width * map_height, empty);

    auto randn = [&](int n) -> int {
        if (n <= 1) return 0;
        return std::uniform_int_distribution<int>(0, n - 1)(rng);
    };
    auto rand01 = [&]() {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng);
    };

    // init_floor_and_walls
    set_area(0, 0, main_width, 1, wall_mid);
    set_area(0, 0, 1, main_height, wall_mid);
    set_area(main_width - 1, 0, 1, main_height, wall_mid);
    set_area(0, main_height - 1, main_width, 1, wall_mid);

    // generate_coin_to_the_right (hard mode)
    int difficulty = randn(3) + 1;
    int min_gap = difficulty - 1;
    int min_plat_w = 1;
    int inc_dy = 4;
    if (cfg.easy_mode) {
        min_gap -= 1;
        if (min_gap < 0) min_gap = 0;
        min_plat_w = 3;
        inc_dy = 2;
    }
    float bomb_prob = 0.25f * (difficulty - 1);
    int max_gap_inc = difficulty == 1 ? 1 : 2;

    int num_sections = randn(difficulty) + difficulty;
    int start_x = 5;
    int curr_x = start_x;
    int curr_y = main_height / 2;
    int min_y = curr_y;
    int w = main_width;

    float _max_dy = max_jump * max_jump / (2.0f * gravity);
    int max_dy = (int)(_max_dy - 0.5f);

    set_area(0, 0, start_x, curr_y, wall_mid);
    set_area(0, curr_y + 8, start_x, main_height - curr_y - 8, wall_mid);

    for (int i = 0; i < num_sections; i++) {
        int prev_x = curr_x;
        int prev_y = curr_y;
        int num_edges = randn(2) + 1;
        int max_y = -1;
        int last_edge_y = -1;

        for (int j = 0; j < num_edges; j++) {
            curr_x = prev_x + j;

            if (curr_x + 15 >= w)
                break;

            curr_y = prev_y;

            int dy = randn(inc_dy) + 1 + (int)(difficulty / 3);
            if (dy > max_dy)
                dy = max_dy;

            if (curr_y >= main_height - 15)
                dy *= -1;
            else if (curr_y >= 5 && rand01() < 0.4f)
                dy *= -1;

            curr_y += dy;

            if (curr_y < 3)
                curr_y = 3;

            if (std::abs(curr_y - last_edge_y) <= 1)
                curr_y = last_edge_y + 2;

            int dx = min_plat_w + randn(3);

            set_area(curr_x, curr_y - 1, dx, 1, wall_mid);

            curr_x += dx;
            curr_x += min_gap + randn(max_gap_inc + 1);

            if (curr_y > max_y)
                max_y = curr_y;
            if (curr_y < min_y)
                min_y = curr_y;

            last_edge_y = curr_y;
        }

        if (rand01() < bomb_prob)
            set(randn(curr_x - prev_x + 1) + prev_x, max_y + 2, bomb_tile);

        int ceiling_height = 11;
        int ceiling_start = max_y - 1 + ceiling_height;
        set_area(prev_x, ceiling_start, curr_x - prev_x, main_height - ceiling_start, wall_mid);
    }

    static const char* shrooms[] = {
        "assets/platformer/shroom1.png",
        "assets/platformer/shroom2.png",
        "assets/platformer/shroom3.png",
        "assets/platformer/shroom4.png",
        "assets/platformer/shroom5.png",
        "assets/platformer/shroom6.png",
    };

    Entity goal = c.create_entity();
    Vector2 gpos{ curr_x + 0.5f, world_y_from_orig(curr_y + 0.5f) };
    c.add_component(goal, Component_Transform{ .position{ gpos } });
    c.add_component(goal, Component_Sprite{ .position{ -0.5f, -0.5f }, .z = 1.0f, .texture = &manager_texture.get(shrooms[randn(6)]) });
    c.add_component(goal, Component_Goal{});
    c.add_component(goal, Component_Collision{ .bounds{ -0.5f, -0.5f, 1.0f, 1.0f } });

    set_area(curr_x, curr_y - 1, 1, 1, wall_mid);
    set_area(curr_x, curr_y + 6, 1, main_height - curr_y - 6, wall_mid);

    int fire_y = min_y - 2;
    if (fire_y < 1)
        fire_y = 1;

    set_area(start_x, 0, main_width - start_x, fire_y, wall_mid);
    set_area(start_x, fire_y, main_width - start_x, 1, fire_tile);
    set_area(curr_x + 1, 0, main_width - curr_x - 1, main_height, wall_mid);
}

void System_Tilemap::render(int theme) {
    Rectangle camera_aabb{
        (gr.camera_position.x - gr.camera_size.x * 0.5f / gr.camera_scale) * pixels_to_unit,
        (gr.camera_position.y - gr.camera_size.y * 0.5f / gr.camera_scale) * pixels_to_unit,
        gr.camera_size.x * pixels_to_unit / gr.camera_scale,
        gr.camera_size.y * pixels_to_unit / gr.camera_scale
    };

    int lower_x = std::floor(camera_aabb.x);
    int lower_y = std::floor(camera_aabb.y);
    int upper_x = std::ceil(camera_aabb.x + camera_aabb.width);
    int upper_y = std::ceil(camera_aabb.y + camera_aabb.height);

    for (int y = lower_y; y <= upper_y; y++) {
        for (int x = lower_x; x <= upper_x; x++) {
            Tile_ID id = get(x, map_height - 1 - y);
            if (id == empty)
                continue;

            Asset_Texture* tex;
            if (id == wall_mid)
                tex = &id_to_textures[id][theme];
            else
                tex = &id_to_textures[id][0];

            gr.render_texture(tex, (Vector2){ x * unit_to_pixels, y * unit_to_pixels }, unit_to_pixels / tex->width);
        }
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

    for (int y = lower_y; y <= upper_y; y++) {
        for (int x = lower_x; x <= upper_x; x++) {
            Tile_ID id = get(x, map_height - 1 - y);
            Collision_Type type = collision_id_func(id);
            if (type == none)
                continue;

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

    for (int y = lower_y; y <= upper_y; y++) {
        for (int x = lower_x; x <= upper_x; x++) {
            Tile_ID id = get(x, map_height - 1 - y);
            Collision_Type type = collision_id_func(id);
            if (type == none)
                continue;

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
