#include "../../cenv/cenv.h"

#include <cassert>
#include <cmath>
#include <ctime>
#include <vector>
#include <algorithm>
#include <random>
#include <string>

#include "renderer.h"
#include "common_assets.h"
#include "helpers.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const int version = 100;

cenv_make_data make_data;
cenv_reset_data reset_data;
cenv_step_data step_data;
cenv_render_data render_data;

cenv_key_value observation;

static bool g_initialized = false;
static bool g_human_mode  = false;
static SDL_Window* sdl_window = nullptr;

const int obs_width  = 64;
const int obs_height = 64;
const int num_actions = 15;

int window_width  = 512;
int window_height = 512;

// Hard mode (original dodgeball.cpp)
const float WORLD_W = 20.0f;
const float WORLD_H = 20.0f;
const float MIXRATE  = 0.5f;
const float MAXSPEED = 0.5f;
const float DECAY    = 0.9f;

const float ENEMY_VEL    = 0.05f;
const float BALL_V_ROT   = (float)M_PI * 0.23f;
const float ENEMY_REWARD = 2.0f;
const float COMPLETION_BONUS = 10.0f;

const int NUM_ENEMY_THEMES = 7;
const int ENEMY_FIRE_DELAY = 50;
const int PLAYER_FIRE_CD   = 7;
const int BALL_EXPIRE      = 50;
const int TIMEOUT          = 1000;
const int NUM_ITERATIONS   = 4;
const int MAX_EXTRA_ENEMIES = 3;

const float THICKNESS = 0.3f * 1.5f;   // 0.45
const float ENEMY_R   = 0.5f * 1.5f;   // 0.75
const float BALL_R    = 0.25f * 1.5f;  // 0.375
const float BALL_VSCALE = 0.25f * 1.5f;
const float PLAYER_R  = 0.75f;
const float EXIT_R    = 0.75f;

enum ObjType {
    OBJ_DEAD = 0,
    OBJ_LAVA,
    OBJ_PLAYER_BALL,
    OBJ_ENEMY,
    OBJ_DOOR,
    OBJ_ENEMY_BALL,
    OBJ_DUST
};

struct Ent {
    ObjType type = OBJ_DEAD;
    float x = 0, y = 0;
    float vx = 0, vy = 0;
    float rx = 0.5f, ry = 0.5f;
    float rotation = 0, vrot = 0;
    float health = 1;
    float alpha = 1;
    float grow_rate = 1;
    float alpha_decay = 1;
    int theme = 0;
    int spawn_time = 0;
    int fire_time = 0;
    int life_time = 0;
    int expire_time = -1;
    bool will_erase = false;
};

struct Room {
    float x, y, w, h;
};

Asset_Texture tex_player;
std::vector<Asset_Texture> tex_enemy;
Asset_Texture tex_pball, tex_eball;
Asset_Texture tex_door_closed, tex_door_open;
Asset_Texture tex_lava;
std::vector<Asset_Texture> tex_dust;
std::vector<Asset_Texture> bg_textures;

std::mt19937 rng;

SDL_Surface*  window_target = nullptr;
SDL_Surface*  obs_target    = nullptr;
SDL_Renderer* window_renderer = nullptr;
SDL_Renderer* obs_renderer    = nullptr;

uint32_t rmask, gmask, bmask, amask;

float player_x, player_y, player_vx, player_vy, player_rot;
int last_move_action = 7;
int last_fire_time = 0;
int num_enemies = 0;
int cur_time = 0;
int bg_index = 0;
bool player_alive = true;

std::vector<Ent> entities;
std::vector<Room> rooms;

void render_game(bool is_obs);
void reset_game();
void cenv_close();

static float rand01() {
    return std::uniform_real_distribution<float>(0.f, 1.f)(rng);
}
static int randn(int n) {
    if (n <= 1) return 0;
    return std::uniform_int_distribution<int>(0, n - 1)(rng);
}
static float rand_pos(float r, float lo, float hi) {
    if (hi - lo <= 2.f * r) return 0.5f * (lo + hi);
    return (hi - lo - 2.f * r) * rand01() + r + lo;
}

static bool aabb(float x1, float y1, float rx1, float ry1,
                 float x2, float y2, float rx2, float ry2) {
    return std::fabs(x1 - x2) < (rx1 + rx2) && std::fabs(y1 - y2) < (ry1 + ry2);
}

static bool hits(const Ent& a, const Ent& b) {
    return aabb(a.x, a.y, a.rx, a.ry, b.x, b.y, b.rx, b.ry);
}

static bool hits_player(const Ent& e) {
    return aabb(e.x, e.y, e.rx, e.ry, player_x, player_y, PLAYER_R, PLAYER_R);
}

static bool lava_at(float x, float y, float rx, float ry, int skip = -1) {
    for (int i = 0; i < (int)entities.size(); i++) {
        if (i == skip) continue;
        const Ent& e = entities[i];
        if (e.will_erase || e.type != OBJ_LAVA) continue;
        if (aabb(x, y, rx, ry, e.x, e.y, e.rx, e.ry)) return true;
    }
    return false;
}

static bool any_overlap(float x, float y, float rx, float ry, bool vs_player) {
    if (vs_player && aabb(x, y, rx, ry, player_x, player_y, PLAYER_R, PLAYER_R))
        return true;
    for (const auto& e : entities) {
        if (e.will_erase) continue;
        if (aabb(x, y, rx, ry, e.x, e.y, e.rx, e.ry)) return true;
    }
    return false;
}

static void face(float& rot, float dx, float dy) {
    if (dx != 0.f || dy != 0.f)
        rot = -std::atan2(dy, dx);
}

static void choose_vel(Ent& e) {
    float vel = ENEMY_VEL * (float)(randn(2) * 2 - 1);
    if (randn(2) == 0) { e.vx = vel; e.vy = 0.f; }
    else               { e.vy = vel; e.vx = 0.f; }
    e.spawn_time = randn(50) + 25;
    face(e.rotation, e.vx, e.vy);
}

int32_t cenv_get_env_version() { return version; }

int32_t cenv_make(const char* render_mode, cenv_option* options, int32_t options_size) {
    if (g_initialized) cenv_close();
    g_initialized = true;

    unsigned int seed = (unsigned int)time(nullptr);
    for (int i = 0; i < options_size; i++) {
        std::string name(options[i].name);
        if (name == "seed")   seed = (unsigned int)options[i].value.i;
        if (name == "width")  window_width  = options[i].value.i;
        if (name == "height") window_height = options[i].value.i;
    }

    make_data.observation_spaces_size = 1;
    make_data.observation_spaces = (cenv_key_value*)malloc(sizeof(cenv_key_value));
    make_data.observation_spaces[0].key = "screen";
    make_data.observation_spaces[0].value_type = CENV_SPACE_TYPE_BOX;
    make_data.observation_spaces[0].value_buffer_size = 2;
    make_data.observation_spaces[0].value_buffer.f = (float*)malloc(2 * sizeof(float));
    make_data.observation_spaces[0].value_buffer.f[0] = 0.f;
    make_data.observation_spaces[0].value_buffer.f[1] = 255.f;

    make_data.action_spaces_size = 1;
    make_data.action_spaces = (cenv_key_value*)malloc(sizeof(cenv_key_value));
    make_data.action_spaces[0].key = "action";
    make_data.action_spaces[0].value_type = CENV_SPACE_TYPE_MULTI_DISCRETE;
    make_data.action_spaces[0].value_buffer_size = 1;
    make_data.action_spaces[0].value_buffer.i = (int32_t*)malloc(sizeof(int32_t));
    make_data.action_spaces[0].value_buffer.i[0] = num_actions;

    observation.key = "screen";
    observation.value_type = CENV_VALUE_TYPE_BYTE;
    observation.value_buffer_size = obs_width * obs_height * 3;
    observation.value_buffer.b = (uint8_t*)malloc(obs_width * obs_height * 3);

    reset_data.observations_size = 1; reset_data.observations = &observation;
    reset_data.infos_size = 0;        reset_data.infos = nullptr;

    step_data.observations_size = 1;  step_data.observations = &observation;
    step_data.reward.f = 0.f;
    step_data.terminated = false; step_data.truncated = false;
    step_data.infos_size = 0;     step_data.infos = nullptr;

    render_data.value_type = CENV_VALUE_TYPE_BYTE;
    render_data.value_buffer_height   = window_height;
    render_data.value_buffer_width    = window_width;
    render_data.value_buffer_channels = 3;
    render_data.value_buffer.b = (uint8_t*)malloc(window_width * window_height * 3);

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask=0xff000000; gmask=0x00ff0000; bmask=0x0000ff00; amask=0x000000ff;
#else
    rmask=0x000000ff; gmask=0x0000ff00; bmask=0x00ff0000; amask=0xff000000;
#endif

    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    std::string render_mode_str(render_mode ? render_mode : "");
    g_human_mode = (render_mode_str == "human");
    if (!g_human_mode)
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");

    SDL_Init(SDL_INIT_VIDEO);

    if (g_human_mode) {
        sdl_window    = SDL_CreateWindow("Dodgeball", window_width, window_height, 0);
        window_target = SDL_GetWindowSurface(sdl_window);
    } else {
        window_target = SDL_CreateSurface(window_width, window_height,
            SDL_GetPixelFormatForMasks(32, rmask, gmask, bmask, amask));
    }
    obs_target = SDL_CreateSurface(obs_width, obs_height,
        SDL_GetPixelFormatForMasks(32, rmask, gmask, bmask, amask));

    window_renderer = SDL_CreateSoftwareRenderer(window_target);
    obs_renderer    = SDL_CreateSoftwareRenderer(obs_target);
    gr.window_renderer = window_renderer;
    gr.obs_renderer    = obs_renderer;

    rng.seed(seed);

    tex_player.load("assets/misc_assets/character12.png");
    tex_enemy.resize(11);
    for (int i = 0; i < 11; i++)
        tex_enemy[i].load("assets/misc_assets/character" + std::to_string(i + 1) + ".png");
    tex_pball.load("assets/misc_assets/ball_soccer1.png");
    tex_eball.load("assets/misc_assets/ball_soccer2.png");
    tex_door_closed.load("assets/misc_assets/blockRed.png");
    tex_door_open.load("assets/misc_assets/blockGreen.png");
    tex_lava.load("assets/misc_assets/tileStone_slope2.png");
    tex_dust.resize(9);
    for (int i = 0; i < 9; i++)
        tex_dust[i].load("assets/misc_assets/spaceEffect" + std::to_string(i + 1) + ".png");

    std::vector<std::string> bg_names = {
        "assets/topdown_backgrounds/backgrounddetailed1.png",
        "assets/topdown_backgrounds/backgrounddetailed2.png",
        "assets/topdown_backgrounds/backgrounddetailed3.png",
        "assets/topdown_backgrounds/backgrounddetailed4.png",
        "assets/topdown_backgrounds/backgrounddetailed5.png",
        "assets/topdown_backgrounds/backgrounddetailed6.png",
        "assets/topdown_backgrounds/backgrounddetailed7.png",
        "assets/topdown_backgrounds/backgrounddetailed8.png",
    };
    bg_textures.resize(bg_names.size());
    for (int i = 0; i < (int)bg_names.size(); i++)
        bg_textures[i].load(bg_names[i]);

    reset_game();
    return 0;
}

static void add_lava(float x, float y, float rx, float ry) {
    Ent e;
    e.type = OBJ_LAVA;
    e.x = x; e.y = y; e.rx = rx; e.ry = ry;
    entities.push_back(e);
}

static void add_room(Room room) {
    float hard_min = 4.f * PLAYER_R + 2.f * THICKNESS + 0.5f;
    float min_dim  = PLAYER_R * 8.f + 0.5f;
    if ((room.w >= min_dim || room.h >= min_dim) && room.w >= hard_min && room.h >= hard_min)
        rooms.push_back(room);
}

static void split_room(Room room) {
    float min_dim = PLAYER_R * 8.f + 0.5f;
    bool will_split_width = rand01() < 0.5f;
    bool choice2 = rand01() < 0.5f;
    if (room.w < min_dim) will_split_width = false;
    if (room.h < min_dim) will_split_width = true;

    float rx = room.x, ry = room.y, rw = room.w, rh = room.h;
    float gap = 0.25f * (float)(randn(3) + 1);
    float pct = 1.f - gap;
    float thickness = THICKNESS;

    if (!will_split_width) {
        float wy, wh, remy;
        if (choice2) {
            wy = ry; remy = ry + pct * rh; wh = pct * rh;
        } else {
            wy = ry + (1.f - pct) * rh; remy = ry; wh = pct * rh;
        }
        add_lava(rx + rw / 2.f, wy + wh / 2.f, thickness, wh / 2.f);
        float nextw = rw / 2.f - thickness;
        add_room({rx, wy, nextw, wh});
        add_room({rx + rw / 2.f + thickness, wy, nextw, wh});
        add_room({rx, remy, rw, rh - wh});
    } else {
        float wx, ww, remx;
        if (choice2) {
            wx = rx; remx = rx + pct * rw; ww = pct * rw;
        } else {
            wx = rx + (1.f - pct) * rw; remx = rx; ww = pct * rw;
        }
        add_lava(wx + ww / 2.f, ry + rh / 2.f, ww / 2.f, thickness);
        float nexth = rh / 2.f - thickness;
        add_room({wx, ry, ww, nexth});
        add_room({wx, ry + rh / 2.f + thickness, ww, nexth});
        add_room({remx, ry, rw - ww, rh});
    }
}

static void spawn_in_rect(Ent& e, float x, float y, float w, float h, bool vs_player) {
    e.x = rand_pos(e.rx, x, x + w);
    e.y = rand_pos(e.ry, y, y + h);
    int count = 0;
    while (any_overlap(e.x, e.y, e.rx, e.ry, vs_player) && count < 400) {
        e.x = rand_pos(e.rx, x, x + w);
        e.y = rand_pos(e.ry, y, y + h);
        count++;
    }
}

static void spawn_in_open(Ent& e, bool vs_player) {
    if (!rooms.empty()) {
        for (int attempt = 0; attempt < 8; attempt++) {
            Room r = rooms[randn((int)rooms.size())];
            spawn_in_rect(e, r.x, r.y, r.w, r.h, vs_player);
            if (!any_overlap(e.x, e.y, e.rx, e.ry, vs_player))
                return;
        }
    }
    spawn_in_rect(e, 0, 0, WORLD_W, WORLD_H, vs_player);
}

void reset_game() {
    entities.clear();
    rooms.clear();
    cur_time = 0;
    last_fire_time = 0;
    last_move_action = 7;
    player_vx = player_vy = 0;
    player_rot = 0;
    player_alive = true;
    player_x = PLAYER_R;
    player_y = PLAYER_R;
    bg_index = randn((int)bg_textures.size());

    rooms.push_back({0.f, 0.f, WORLD_W, WORLD_H});
    for (int it = 0; it < NUM_ITERATIONS; it++) {
        if (rooms.empty()) break;
        int idx = randn((int)rooms.size());
        Room room = rooms[idx];
        rooms.erase(rooms.begin() + idx);
        split_room(room);
    }

    float doorlen = 2.f * EXIT_R;
    Ent door;
    door.type = OBJ_DOOR;
    int wall = randn(4);
    if (wall == 0) {
        door.rx = doorlen / 2.f; door.ry = EXIT_R;
        spawn_in_rect(door, 0, 0, WORLD_W, 2.f * EXIT_R, false);
    } else if (wall == 1) {
        door.rx = doorlen / 2.f; door.ry = EXIT_R;
        spawn_in_rect(door, 0, WORLD_H - 2.f * EXIT_R, WORLD_W, 2.f * EXIT_R, false);
    } else if (wall == 2) {
        door.rx = EXIT_R; door.ry = doorlen / 2.f;
        spawn_in_rect(door, 0, 0, 2.f * EXIT_R, WORLD_H, false);
    } else {
        door.rx = EXIT_R; door.ry = doorlen / 2.f;
        spawn_in_rect(door, WORLD_W - 2.f * EXIT_R, 0, 2.f * EXIT_R, WORLD_H, false);
    }
    entities.push_back(door);

    Ent agent_probe;
    agent_probe.rx = agent_probe.ry = PLAYER_R;
    spawn_in_open(agent_probe, false);
    player_x = agent_probe.x;
    player_y = agent_probe.y;
    face(player_rot, 1.f, 0.f);

    num_enemies = randn(MAX_EXTRA_ENEMIES + 1) + 3;
    int enemy_theme = randn(NUM_ENEMY_THEMES);
    for (int i = 0; i < num_enemies; i++) {
        Ent e;
        e.type = OBJ_ENEMY;
        e.rx = e.ry = ENEMY_R;
        e.health = 1;
        e.theme = enemy_theme;
        e.fire_time = 10;
        spawn_in_open(e, true);
        choose_vel(e);
        e.spawn_time = 0;
        entities.push_back(e);
    }
}

static void copy_surface_rgb(SDL_Surface* src, uint8_t* dst, int w, int h) {
    SDL_LockSurface(src);
    uint8_t* px = (uint8_t*)src->pixels;
    for (int x = 0; x < w; x++)
        for (int y = 0; y < h; y++) {
            dst[0 + 3 * (y + h * x)] = px[0 + 4 * (y + h * x)];
            dst[1 + 3 * (y + h * x)] = px[1 + 4 * (y + h * x)];
            dst[2 + 3 * (y + h * x)] = px[2 + 4 * (y + h * x)];
        }
    SDL_UnlockSurface(src);
}

static void fire_ball(const Ent& src, float vx, float vy, ObjType type) {
    Ent b;
    b.type = type;
    b.x = src.x; b.y = src.y;
    b.vx = vx * BALL_VSCALE;
    b.vy = vy * BALL_VSCALE;
    b.rx = b.ry = BALL_R;
    b.vrot = BALL_V_ROT;
    b.expire_time = BALL_EXPIRE;
    entities.push_back(b);
}

static void try_move(Ent& e, int idx, bool reflect_lava) {
    auto blocked = [&](float nx, float ny) {
        if (nx - e.rx < 0.f || nx + e.rx > WORLD_W ||
            ny - e.ry < 0.f || ny + e.ry > WORLD_H)
            return true;
        return reflect_lava && lava_at(nx, ny, e.rx, e.ry, idx);
    };

    float nx = e.x + e.vx;
    if (e.vx != 0.f && blocked(nx, e.y)) {
        if (reflect_lava) e.vx = -e.vx;
        nx = e.x;
    }
    float ny = e.y + e.vy;
    if (e.vy != 0.f && blocked(e.x, ny)) {
        if (reflect_lava) e.vy = -e.vy;
        ny = e.y;
    }
    e.x = nx;
    e.y = ny;
}

int32_t cenv_step(cenv_key_value* actions, int32_t actions_size) {
    int action = 4;
    for (int i = 0; i < actions_size; i++) {
        std::string key(actions[i].key);
        if (key == "action") {
            assert(actions[i].value_type == CENV_VALUE_TYPE_INT);
            action = actions[i].value_buffer.i[0];
        }
    }

    cur_time++;

    int move_action = action % 9;
    int special_action = 0;
    if (action >= 9) {
        special_action = action - 8;
        move_action = 4;
    }
    if (move_action != 4)
        last_move_action = move_action;

    float avx = (float)(move_action / 3 - 1);
    float avy = (float)(move_action % 3 - 1);

    player_vx = (1.f - MIXRATE) * player_vx + MIXRATE * MAXSPEED * avx;
    player_vy = (1.f - MIXRATE) * player_vy + MIXRATE * MAXSPEED * avy;
    player_vx *= DECAY;
    player_vy *= DECAY;

    float lvx = (float)(last_move_action / 3 - 1);
    float lvy = (float)(last_move_action % 3 - 1);
    face(player_rot, lvx, lvy);

    float nx = player_x + player_vx;
    float ny = player_y + player_vy;
    nx = std::max(PLAYER_R, std::min(WORLD_W - PLAYER_R, nx));
    ny = std::max(PLAYER_R, std::min(WORLD_H - PLAYER_R, ny));
    player_x = nx;
    player_y = ny;

    float reward = 0.f;
    bool terminated = false;
    bool truncated = false;

    if (special_action == 1 && (cur_time - last_fire_time) >= PLAYER_FIRE_CD) {
        Ent dummy; dummy.x = player_x; dummy.y = player_y;
        fire_ball(dummy, lvx, lvy, OBJ_PLAYER_BALL);
        last_fire_time = cur_time;
    }

    struct Shot { float x, y, vx, vy; };
    std::vector<Shot> pending_shots;

    num_enemies = 0;
    for (int i = 0; i < (int)entities.size(); i++) {
        Ent& e = entities[i];
        if (e.will_erase) continue;

        if (e.type == OBJ_ENEMY) {
            num_enemies++;
            if (e.spawn_time == 0) choose_vel(e);
            else e.spawn_time -= 1;

            try_move(e, i, true);
            face(e.rotation, e.vx, e.vy);

            bool can_fire = (cur_time - e.fire_time) >= ENEMY_FIRE_DELAY;
            if (can_fire) {
                float dx = e.x - player_x;
                float dy = e.y - player_y;
                float bvx = (e.x < player_x ? 1.f : -1.f);
                float bvy = (e.y < player_y ? 1.f : -1.f);
                if (std::fabs(dx) < 1.f) {
                    pending_shots.push_back({e.x, e.y, 0.f, bvy});
                    e.fire_time = cur_time + randn(4);
                    e.vx = 0; e.vy = bvy * ENEMY_VEL;
                } else if (std::fabs(dy) < 1.f) {
                    pending_shots.push_back({e.x, e.y, bvx, 0.f});
                    e.fire_time = cur_time + randn(4);
                    e.vx = bvx * ENEMY_VEL; e.vy = 0;
                }
            }
        } else if (e.type == OBJ_PLAYER_BALL || e.type == OBJ_ENEMY_BALL) {
            e.x += e.vx;
            e.y += e.vy;
            e.rotation += e.vrot;
            if (e.x < e.rx || e.x > WORLD_W - e.rx ||
                e.y < e.ry || e.y > WORLD_H - e.ry ||
                lava_at(e.x, e.y, e.rx, e.ry))
                e.will_erase = true;
        } else if (e.type == OBJ_DUST) {
            e.rx *= e.grow_rate;
            e.ry *= e.grow_rate;
            e.alpha *= e.alpha_decay;
            e.rotation += e.vrot;
        }

        e.life_time++;
        if (e.expire_time > 0 && e.life_time > e.expire_time)
            e.will_erase = true;
    }

    for (const auto& s : pending_shots) {
        Ent dummy; dummy.x = s.x; dummy.y = s.y;
        fire_ball(dummy, s.vx, s.vy, OBJ_ENEMY_BALL);
    }

    // Agent collisions
    for (auto& e : entities) {
        if (e.will_erase || !player_alive) continue;
        if (!hits_player(e)) continue;
        if (e.type == OBJ_ENEMY || e.type == OBJ_ENEMY_BALL || e.type == OBJ_LAVA) {
            player_alive = false;
            terminated = true;
        } else if (e.type == OBJ_DOOR && num_enemies == 0) {
            reward += COMPLETION_BONUS;
            terminated = true;
        }
    }

    // Player-ball vs enemies (src=enemy has collides_with_entities in original)
    for (auto& ball : entities) {
        if (ball.will_erase || ball.type != OBJ_PLAYER_BALL) continue;
        for (auto& tgt : entities) {
            if (tgt.will_erase || &tgt == &ball) continue;
            if (!hits(ball, tgt)) continue;
            if (tgt.type == OBJ_LAVA) {
                ball.will_erase = true;
            } else if (tgt.type == OBJ_ENEMY) {
                tgt.health -= 1.f;
                ball.will_erase = true;
                if (tgt.health <= 0.f && !tgt.will_erase) {
                    tgt.will_erase = true;
                    reward += ENEMY_REWARD;
                    Ent dust;
                    dust.type = OBJ_DUST;
                    dust.x = tgt.x; dust.y = tgt.y;
                    dust.rx = dust.ry = tgt.rx;
                    dust.vrot = (float)M_PI / 0.3f;
                    dust.grow_rate = 1.f / 1.2f;
                    dust.expire_time = 4;
                    dust.alpha_decay = 0.9f;
                    dust.theme = randn((int)tex_dust.size());
                    entities.push_back(dust);
                    num_enemies = std::max(0, num_enemies - 1);
                }
            }
        }
    }

    entities.erase(std::remove_if(entities.begin(), entities.end(),
        [](const Ent& e){ return e.will_erase; }), entities.end());

    num_enemies = 0;
    for (const auto& e : entities)
        if (e.type == OBJ_ENEMY) num_enemies++;

    if (!terminated && cur_time >= TIMEOUT)
        truncated = true;

    step_data.reward.f   = reward;
    step_data.terminated = terminated;
    step_data.truncated  = truncated;

    render_game(true);
    copy_surface_rgb(obs_target, observation.value_buffer.b, obs_width, obs_height);
    return 0;
}

int32_t cenv_reset(cenv_option* options, int32_t options_size) {
    for (int i = 0; i < options_size; i++) {
        std::string name(options[i].name);
        if (name == "seed") rng.seed((unsigned int)options[i].value.i);
    }
    reset_game();
    render_game(true);
    copy_surface_rgb(obs_target, observation.value_buffer.b, obs_width, obs_height);
    return 0;
}

static void blit(Asset_Texture* tex, float x, float y, float rx, float ry,
                 float cam_x, float cam_y, float pix, int w, int h,
                 float rotation, float alpha) {
    if (!tex || tex->width == 0) return;
    SDL_FRect dst;
    dst.w = 2.f * rx * pix;
    dst.h = 2.f * ry * pix;
    dst.x = (x - cam_x) * pix + 0.5f * (float)w - rx * pix;
    dst.y = (cam_y - y) * pix + 0.5f * (float)h - ry * pix;
    SDL_Texture* t = gr.rendering_obs ? tex->obs_texture : tex->window_texture;
    if (alpha < 1.f)
        SDL_SetTextureAlphaMod(t, (Uint8)(255 * std::max(0.f, alpha)));
    SDL_RenderTextureRotated(gr.get_renderer(), t, nullptr, &dst,
                             rotation * 180.f / (float)M_PI, nullptr, SDL_FLIP_NONE);
    if (alpha < 1.f)
        SDL_SetTextureAlphaMod(t, 255);
}

void render_game(bool is_obs) {
    gr.rendering_obs = is_obs;
    int width  = is_obs ? obs_width  : window_width;
    int height = is_obs ? obs_height : window_height;

    SDL_SetRenderDrawColor(gr.get_renderer(), 0, 0, 0, 255);
    SDL_RenderClear(gr.get_renderer());

    float pix  = (float)height / WORLD_H;
    float cam_x = WORLD_W * 0.5f;
    float cam_y = WORLD_H * 0.5f;

    if (!bg_textures.empty()) {
        Asset_Texture* bg = &bg_textures[bg_index];
        if (bg->width > 0) {
            SDL_FRect dst{ 0, 0, (float)width, (float)height };
            SDL_Texture* t = is_obs ? bg->obs_texture : bg->window_texture;
            SDL_RenderTexture(gr.get_renderer(), t, nullptr, &dst);
        }
    }

    for (const auto& e : entities) {
        Asset_Texture* tex = nullptr;
        switch (e.type) {
            case OBJ_LAVA:        tex = &tex_lava; break;
            case OBJ_PLAYER_BALL: tex = &tex_pball; break;
            case OBJ_ENEMY_BALL:  tex = &tex_eball; break;
            case OBJ_ENEMY:       tex = &tex_enemy[e.theme % (int)tex_enemy.size()]; break;
            case OBJ_DOOR:
                tex = (num_enemies == 0) ? &tex_door_open : &tex_door_closed;
                break;
            case OBJ_DUST:        tex = &tex_dust[e.theme % (int)tex_dust.size()]; break;
            default: break;
        }
        if (tex)
            blit(tex, e.x, e.y, e.rx, e.ry, cam_x, cam_y, pix, width, height,
                 e.rotation, e.alpha);
    }

    if (player_alive)
        blit(&tex_player, player_x, player_y, PLAYER_R, PLAYER_R,
             cam_x, cam_y, pix, width, height, player_rot, 1.f);

    SDL_RenderPresent(gr.get_renderer());
}

int32_t cenv_render() {
    render_game(false);
    if (g_human_mode) {
        SDL_UpdateWindowSurface(sdl_window);
        SDL_PumpEvents();
        SDL_Delay(1000 / 15);
        return 0;
    }
    copy_surface_rgb(window_target, render_data.value_buffer.b, window_width, window_height);
    return 0;
}

void cenv_close() {
    if (!g_initialized) return;

    entities.clear();
    tex_player = Asset_Texture();
    tex_enemy.clear();
    tex_pball = Asset_Texture();
    tex_eball = Asset_Texture();
    tex_door_closed = Asset_Texture();
    tex_door_open = Asset_Texture();
    tex_lava = Asset_Texture();
    tex_dust.clear();
    bg_textures.clear();
    manager_texture.clear();

    SDL_DestroyRenderer(window_renderer);
    window_renderer    = nullptr;
    gr.window_renderer = nullptr;
    if (g_human_mode) {
        SDL_DestroyWindow(sdl_window);
        sdl_window = nullptr;
    } else {
        SDL_DestroySurface(window_target);
    }
    window_target = nullptr;

    SDL_DestroyRenderer(obs_renderer);
    obs_renderer    = nullptr;
    gr.obs_renderer = nullptr;
    SDL_DestroySurface(obs_target);
    obs_target = nullptr;

    SDL_Quit();

    free(make_data.observation_spaces[0].value_buffer.f);
    free(make_data.observation_spaces);
    free(make_data.action_spaces[0].value_buffer.i);
    free(make_data.action_spaces);
    free(observation.value_buffer.b); observation.value_buffer.b = nullptr;
    free(render_data.value_buffer.b); render_data.value_buffer.b = nullptr;

    g_human_mode  = false;
    g_initialized = false;
}
