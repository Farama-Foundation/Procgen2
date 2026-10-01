#include "../../cenv/cenv.h"
#include "../../cenv/distribution_mode.h"

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
int distribution_mode = DIST_HARD;

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

// Hard mode
const int WORLD_DIM = 15;
const int NSTEP = 5;
const float MAX_SPEED = 2.f / (NSTEP - 1.f);  // 0.5
const float VEL_DECAY = MAX_SPEED / NSTEP;    // 0.1
const float PLAYER_R = 0.4f;
const float MONSTER_RADIUS = 0.25f;
const float LOG_RADIUS = 0.45f;
const float GOAL_REWARD = 10.f;
const int TIMEOUT = 500;
const int FROG_FRAMES = NSTEP;

float min_car_speed = 0.05f;
float max_car_speed = 0.2f;
float min_log_speed = 0.05f;
float max_log_speed = 0.1f;

enum Tile { TILE_GRASS = 0, TILE_ROAD, TILE_WATER };
enum EntType { ENT_CAR = 0, ENT_LOG, ENT_FINISH };

struct Ent {
    EntType type;
    float x, y, vx, vy, rx, ry;
    int theme = 0;
    float rotation = 0;
    bool will_erase = false;
};

Asset_Texture tex_road, tex_water, tex_log, tex_finish;
std::vector<Asset_Texture> tex_car, tex_frog;
std::vector<Asset_Texture> bg_textures;

std::mt19937 rng;
SDL_Surface*  window_target = nullptr;
SDL_Surface*  obs_target    = nullptr;
SDL_Renderer* window_renderer = nullptr;
SDL_Renderer* obs_renderer    = nullptr;
uint32_t rmask, gmask, bmask, amask;

float player_x, player_y, player_vx, player_vy, player_rot;
int frog_theme = 0;
int world_w = WORLD_DIM, world_h = WORLD_DIM;
int bottom_road_y = 1, bottom_water_y = 1, goal_y = 1;
std::vector<float> road_lane_speeds, water_lane_speeds;
std::vector<int> tiles;
std::vector<Ent> entities;
int cur_time = 0;
int bg_index = 0;

void render_game(bool is_obs);
void reset_game();
void cenv_close();

static float rand01() { return std::uniform_real_distribution<float>(0.f, 1.f)(rng); }
static int randn(int n) {
    if (n <= 1) return 0;
    return std::uniform_int_distribution<int>(0, n - 1)(rng);
}
static float randrange(float lo, float hi) {
    return std::uniform_real_distribution<float>(lo, hi)(rng);
}
static float rand_sign() { return rand01() < 0.5f ? 1.f : -1.f; }
static float sgn(float x) { return x > 0.f ? 1.f : (x < 0.f ? -1.f : 0.f); }

static int tile_at(int x, int y) {
    if (x < 0 || y < 0 || x >= world_w || y >= world_h) return TILE_GRASS;
    return tiles[y * world_w + x];
}
static int tile_at_pt(float x, float y) {
    return tile_at((int)std::floor(x), (int)std::floor(y));
}
static void fill_row(int y, Tile t) {
    if (y < 0 || y >= world_h) return;
    for (int x = 0; x < world_w; x++)
        tiles[y * world_w + x] = t;
}

static bool aabb(float x1, float y1, float rx1, float ry1,
                 float x2, float y2, float rx2, float ry2, float margin = 0.f) {
    return std::fabs(x1 - x2) < (rx1 + rx2 + margin)
        && std::fabs(y1 - y2) < (ry1 + ry2 + margin);
}

static bool hits(const Ent& a, const Ent& b, float margin = 0.f) {
    return aabb(a.x, a.y, a.rx, a.ry, b.x, b.y, b.rx, b.ry, margin);
}

static bool oob_player() {
    return (player_x + PLAYER_R < 0.f) || (player_y + PLAYER_R < 0.f)
        || (player_x - PLAYER_R > (float)world_w) || (player_y - PLAYER_R > (float)world_h);
}

static bool oob_ent(const Ent& e) {
    return (e.x + e.rx < 0.f) || (e.y + e.ry < 0.f)
        || (e.x - e.rx > (float)world_w) || (e.y - e.ry > (float)world_h);
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
        if (name == "distribution_mode") distribution_mode = options[i].value.i;
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
    reset_data.infos_size = 0; reset_data.infos = nullptr;
    step_data.observations_size = 1;  step_data.observations = &observation;
    step_data.reward.f = 0.f;
    step_data.terminated = false; step_data.truncated = false;
    step_data.infos_size = 0; step_data.infos = nullptr;
    render_data.value_type = CENV_VALUE_TYPE_BYTE;
    render_data.value_buffer_height = window_height;
    render_data.value_buffer_width  = window_width;
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
        sdl_window    = SDL_CreateWindow("Leaper", window_width, window_height, 0);
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

    tex_road.load("assets/misc_assets/roadTile6b.png");
    tex_water.load("assets/misc_assets/terrainTile6.png");
    tex_log.load("assets/misc_assets/elementWood044.png");
    tex_finish.load("assets/misc_assets/finish2.png");
    tex_car.resize(5);
    tex_car[0].load("assets/misc_assets/car_yellow_5.png");
    tex_car[1].load("assets/misc_assets/car_black_1.png");
    tex_car[2].load("assets/misc_assets/car_blue_2.png");
    tex_car[3].load("assets/misc_assets/car_green_3.png");
    tex_car[4].load("assets/misc_assets/car_red_4.png");
    tex_frog.resize(5);
    tex_frog[0].load("assets/misc_assets/frog1.png");
    tex_frog[1].load("assets/misc_assets/frog2.png");
    tex_frog[2].load("assets/misc_assets/frog4.png");
    tex_frog[3].load("assets/misc_assets/frog6.png");
    tex_frog[4].load("assets/misc_assets/frog7.png");

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

static bool any_overlap(const Ent& m) {
    for (const auto& e : entities) {
        if (e.will_erase) continue;
        if (hits(m, e)) return true;
    }
    return false;
}

static void spawn_entities() {
    for (int lane = 0; lane < (int)road_lane_speeds.size(); lane++) {
        float speed = road_lane_speeds[lane];
        float spawn_prob = std::fabs(speed) / 6.f;
        if (rand01() < spawn_prob) {
            Ent m;
            m.type = ENT_CAR;
            m.x = speed > 0.f ? -MONSTER_RADIUS : (world_w + MONSTER_RADIUS);
            m.y = bottom_road_y + lane + 0.5f;
            m.vx = speed; m.vy = 0;
            m.rx = 2.f * MONSTER_RADIUS;
            m.ry = MONSTER_RADIUS;
            m.theme = randn((int)tex_car.size());
            m.rotation = speed < 0.f ? (float)M_PI : 0.f;
            if (!any_overlap(m))
                entities.push_back(m);
        }
    }
    for (int lane = 0; lane < (int)water_lane_speeds.size(); lane++) {
        float speed = water_lane_speeds[lane];
        float spawn_prob = std::fabs(speed) / 2.f;
        if (rand01() < spawn_prob) {
            Ent m;
            m.type = ENT_LOG;
            m.x = speed > 0.f ? -LOG_RADIUS : (world_w + LOG_RADIUS);
            m.y = bottom_water_y + lane + 0.5f;
            m.vx = speed; m.vy = 0;
            m.rx = m.ry = LOG_RADIUS;
            if (!any_overlap(m))
                entities.push_back(m);
        }
    }
}

static void decay_vel(float& vel) {
    float s = sgn(vel);
    vel = std::fabs(vel) - VEL_DECAY;
    if (vel < 0.f) vel = 0.f;
    vel *= s;
}

void reset_game() {
    entities.clear();
    cur_time = 0;
    player_vx = player_vy = 0;
    player_rot = 0;
    frog_theme = 0;
    bg_index = randn((int)bg_textures.size());
    if (distribution_mode == DIST_EASY) {
        world_w = world_h = 9;
        min_car_speed = 0.03f; max_car_speed = 0.12f;
        min_log_speed = 0.025f; max_log_speed = 0.075f;
    } else if (distribution_mode == DIST_EXTREME) {
        world_w = world_h = 20;
        min_car_speed = 0.1f; max_car_speed = 0.3f;
        min_log_speed = 0.1f; max_log_speed = 0.2f;
    } else {
        world_w = world_h = 15;
        min_car_speed = 0.05f; max_car_speed = 0.2f;
        min_log_speed = 0.05f; max_log_speed = 0.1f;
    }

    tiles.assign(world_w * world_h, TILE_GRASS);

    player_x = rand01() * (world_w - 2.f * PLAYER_R) + PLAYER_R;
    player_y = PLAYER_R;

    int extra_space_road = (distribution_mode == DIST_EASY) ? 0 : randn(2);
    bottom_road_y = extra_space_road + 1;
    int max_diff = (distribution_mode == DIST_EASY) ? 3 : 4;
    int difficulty = randn(max_diff + 1);
    int extra_lane = (distribution_mode == DIST_EASY) ? 0 : randn(4);
    int num_road = difficulty + (extra_lane == 2 ? 1 : 0);
    road_lane_speeds.clear();
    for (int lane = 0; lane < num_road; lane++) {
        road_lane_speeds.push_back(rand_sign() * randrange(min_car_speed, max_car_speed));
        fill_row(bottom_road_y + lane, TILE_ROAD);
    }

    int extra_space_water = (distribution_mode == DIST_EASY) ? 0 : randn(2);
    bottom_water_y = bottom_road_y + num_road + extra_space_water + 1;
    int num_water = difficulty + (extra_lane == 3 ? 1 : 0);
    water_lane_speeds.clear();
    float curr_sign = rand_sign();
    for (int lane = 0; lane < num_water; lane++) {
        water_lane_speeds.push_back(curr_sign * randrange(min_log_speed, max_log_speed));
        curr_sign *= -1.f;
        fill_row(bottom_water_y + lane, TILE_WATER);
    }

    goal_y = bottom_water_y + num_water + 1;

    float min_speed = std::min(min_car_speed, min_log_speed);
    int warm = (int)(world_w / min_speed);
    for (int i = 0; i < warm; i++) {
        spawn_entities();
        for (auto& e : entities) {
            e.x += e.vx;
            e.y += e.vy;
            if (oob_ent(e)) e.will_erase = true;
        }
        entities.erase(std::remove_if(entities.begin(), entities.end(),
            [](const Ent& e){ return e.will_erase; }), entities.end());
    }

    Ent fin;
    fin.type = ENT_FINISH;
    fin.x = world_w / 2.f;
    fin.y = goal_y - 0.5f;
    fin.vx = fin.vy = 0;
    fin.rx = world_w / 2.f;
    fin.ry = 0.5f;
    entities.push_back(fin);
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

int32_t cenv_step(cenv_key_value* actions, int32_t actions_size) {
    int action = 4;
    for (int i = 0; i < actions_size; i++) {
        std::string key(actions[i].key);
        if (key == "action") {
            assert(actions[i].value_type == CENV_VALUE_TYPE_INT);
            action = actions[i].value_buffer.i[0];
        }
    }

    if (frog_theme >= 1)
        frog_theme = (frog_theme + 1) % FROG_FRAMES;

    int move_action = action % 9;
    if (action >= 9) move_action = 4;
    float avx = (float)(move_action / 3 - 1);
    float avy = (float)(move_action % 3 - 1);

    if (player_vx == 0.f && player_vy == 0.f) {
        if (avx != 0.f) {
            player_vx = MAX_SPEED * avx;
            frog_theme = 1;
            player_rot = (player_vx > 0.f ? 1.f : -1.f) * (float)M_PI / 2.f;
        } else if (avy != 0.f) {
            player_vy = MAX_SPEED * avy;
            frog_theme = 1;
            player_rot = player_vy > 0.f ? 0.f : (float)M_PI;
        }
    }
    decay_vel(player_vx);
    decay_vel(player_vy);

    player_x += player_vx;
    player_y += player_vy;

    for (auto& e : entities) {
        if (e.type == ENT_FINISH) continue;
        e.x += e.vx;
        e.y += e.vy;
        if (oob_ent(e)) e.will_erase = true;
    }

    float reward = 0.f;
    bool terminated = false;

    for (auto& e : entities) {
        if (e.will_erase) continue;
        if (!aabb(player_x, player_y, PLAYER_R, PLAYER_R, e.x, e.y, e.rx, e.ry))
            continue;
        if (e.type == ENT_CAR) {
            terminated = true;
        } else if (e.type == ENT_FINISH && player_vx == 0.f && player_vy == 0.f) {
            reward += GOAL_REWARD;
            terminated = true;
        }
    }

    spawn_entities();

    bool standing_on_log = false;
    float log_vx = 0.f;
    float margin = -PLAYER_R;
    for (const auto& m : entities) {
        if (m.type != ENT_LOG || m.will_erase) continue;
        if (aabb(player_x, player_y, PLAYER_R, PLAYER_R, m.x, m.y, m.rx, m.ry, margin)) {
            standing_on_log = true;
            log_vx = m.vx;
        }
    }

    if (!terminated && tile_at_pt(player_x, player_y) == TILE_WATER) {
        if (!standing_on_log && player_vx == 0.f && player_vy == 0.f)
            terminated = true;
    }
    if (standing_on_log)
        player_x += log_vx;
    if (!terminated && oob_player())
        terminated = true;

    entities.erase(std::remove_if(entities.begin(), entities.end(),
        [](const Ent& e){ return e.will_erase; }), entities.end());

    cur_time++;
    bool truncated = !terminated && cur_time >= TIMEOUT;

    step_data.reward.f = reward;
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
        else if (name == "distribution_mode" && options[i].value_type == CENV_VALUE_TYPE_INT) distribution_mode = options[i].value.i;
    }
    reset_game();
    render_game(true);
    copy_surface_rgb(obs_target, observation.value_buffer.b, obs_width, obs_height);
    return 0;
}

static void blit(Asset_Texture* tex, float x, float y, float rx, float ry,
                 float cam_x, float cam_y, float pix, int w, int h, float rotation) {
    if (!tex || tex->width == 0) return;
    SDL_FRect dst;
    dst.w = 2.f * rx * pix;
    dst.h = 2.f * ry * pix;
    dst.x = (x - cam_x) * pix + 0.5f * (float)w - rx * pix;
    dst.y = (cam_y - y) * pix + 0.5f * (float)h - ry * pix;
    SDL_Texture* t = gr.rendering_obs ? tex->obs_texture : tex->window_texture;
    SDL_RenderTextureRotated(gr.get_renderer(), t, nullptr, &dst,
                             rotation * 180.f / (float)M_PI, nullptr, SDL_FLIP_NONE);
}

void render_game(bool is_obs) {
    gr.rendering_obs = is_obs;
    int width  = is_obs ? obs_width  : window_width;
    int height = is_obs ? obs_height : window_height;

    SDL_SetRenderDrawColor(gr.get_renderer(), 0, 0, 0, 255);
    SDL_RenderClear(gr.get_renderer());

    float view = (float)world_h;
    float pix  = (float)height / view;
    float cam_x = 0.5f * (float)world_w;
    float cam_y = 0.5f * (float)world_h;

    if (!bg_textures.empty()) {
        Asset_Texture* bg = &bg_textures[bg_index];
        if (bg->width > 0) {
            SDL_FRect dst{ 0, 0, (float)width, (float)height };
            SDL_Texture* t = is_obs ? bg->obs_texture : bg->window_texture;
            SDL_RenderTexture(gr.get_renderer(), t, nullptr, &dst);
        }
    }

    float wr = 0.5f;
    for (int gx = 0; gx < world_w; gx++) {
        for (int gy = 0; gy < world_h; gy++) {
            int t = tile_at(gx, gy);
            if (t == TILE_GRASS) continue;
            Asset_Texture* tex = (t == TILE_ROAD) ? &tex_road : &tex_water;
            blit(tex, gx + 0.5f, gy + 0.5f, wr, wr, cam_x, cam_y, pix, width, height, 0.f);
        }
    }

    for (const auto& e : entities) {
        if (e.type == ENT_FINISH) {
            blit(&tex_finish, e.x, e.y, e.rx, e.ry, cam_x, cam_y, pix, width, height, 0.f);
        } else if (e.type == ENT_LOG) {
            blit(&tex_log, e.x, e.y, e.rx, e.ry, cam_x, cam_y, pix, width, height, 0.f);
        } else if (e.type == ENT_CAR) {
            blit(&tex_car[e.theme % (int)tex_car.size()], e.x, e.y, e.rx, e.ry,
                 cam_x, cam_y, pix, width, height, e.rotation);
        }
    }

    Asset_Texture* frog = &tex_frog[frog_theme % (int)tex_frog.size()];
    // Original stretches the frog sprite slightly taller
    blit(frog, player_x, player_y, PLAYER_R, PLAYER_R * 1.2f,
         cam_x, cam_y, pix, width, height, player_rot);

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
    tex_road = Asset_Texture();
    tex_water = Asset_Texture();
    tex_log = Asset_Texture();
    tex_finish = Asset_Texture();
    tex_car.clear();
    tex_frog.clear();
    bg_textures.clear();
    manager_texture.clear();

    SDL_DestroyRenderer(window_renderer);
    window_renderer = nullptr; gr.window_renderer = nullptr;
    if (g_human_mode) { SDL_DestroyWindow(sdl_window); sdl_window = nullptr; }
    else SDL_DestroySurface(window_target);
    window_target = nullptr;
    SDL_DestroyRenderer(obs_renderer);
    obs_renderer = nullptr; gr.obs_renderer = nullptr;
    SDL_DestroySurface(obs_target); obs_target = nullptr;
    SDL_Quit();

    free(make_data.observation_spaces[0].value_buffer.f);
    free(make_data.observation_spaces);
    free(make_data.action_spaces[0].value_buffer.i);
    free(make_data.action_spaces);
    free(observation.value_buffer.b); observation.value_buffer.b = nullptr;
    free(render_data.value_buffer.b); render_data.value_buffer.b = nullptr;
    g_human_mode = false;
    g_initialized = false;
}
