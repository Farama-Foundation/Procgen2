#include "../../cenv/cenv.h"
#include "../../cenv/distribution_mode.h"

#include <cassert>
#include <cmath>
#include <ctime>
#include <vector>
#include <algorithm>
#include <numeric>
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

// Hard mode (original plunder.cpp)
const float WORLD_W = 20.f;
const float WORLD_H = 20.f;
const float MIXRATE = 0.5f;
const float MAXSPEED = 0.85f;
const float DECAY = 0.9f;
const float COMPLETION_BONUS = 10.f;
const float POSITIVE_REWARD = 1.f;
const int TIMEOUT = 4000;
float R_SCALE = 1.0f;
const float SPAWN_PROB = 0.06f;
const int NUM_LANES = 5;
const int NUM_SHIP_TYPES = 6;
const int NUM_CURRENT_TYPES = 2;
const int TARGET_QUOTA = 20;

enum EntType {
    ENT_SHIP = 0,
    ENT_BULLET,
    ENT_PANEL,
    ENT_LEGEND_BG,
    ENT_LEGEND,
    ENT_EXPLOSION
};

struct Ent {
    EntType type;
    float x, y, vx, vy, rx, ry;
    int theme = 0;
    float rotation = 0.f;
    bool reflected = false;
    int expire = -1;
    bool will_erase = false;
};

std::vector<Asset_Texture> tex_ship;
Asset_Texture tex_bullet, tex_panel, tex_target_bg, tex_explosion;
std::vector<Asset_Texture> bg_textures;

std::mt19937 rng;
SDL_Surface*  window_target = nullptr;
SDL_Surface*  obs_target    = nullptr;
SDL_Renderer* window_renderer = nullptr;
SDL_Renderer* obs_renderer    = nullptr;
uint32_t rmask, gmask, bmask, amask;

float player_x, player_y, player_vx, player_vy, player_rx, player_ry;
int player_theme = 0;
float player_rot = 0.f;
int last_fire_time = 0;
int cur_time = 0;
int bg_index = 0;

int num_lanes = NUM_LANES;
std::vector<int> lane_directions;
std::vector<float> lane_vels;
std::vector<int> image_permutation;
std::vector<char> target_bools;
int targets_hit = 0;
int target_quota = TARGET_QUOTA;
float juice_left = 1.f;
float legend_r = 2.f;
float min_agent_x = 0.f;
std::vector<Ent> entities;

void render_game(bool is_obs);
void reset_game();
void cenv_close();

static float rand01() { return std::uniform_real_distribution<float>(0.f, 1.f)(rng); }
static int randn(int n) {
    if (n <= 1) return 0;
    return std::uniform_int_distribution<int>(0, n - 1)(rng);
}

static bool aabb(float x1, float y1, float rx1, float ry1,
                 float x2, float y2, float rx2, float ry2) {
    return std::fabs(x1 - x2) < (rx1 + rx2) && std::fabs(y1 - y2) < (ry1 + ry2);
}

static bool hits(const Ent& a, const Ent& b) {
    return aabb(a.x, a.y, a.rx, a.ry, b.x, b.y, b.rx, b.ry);
}

static bool hits_player(const Ent& e) {
    return aabb(player_x, player_y, player_rx, player_ry, e.x, e.y, e.rx, e.ry);
}

static void match_aspect(Ent& e, const Asset_Texture& tex) {
    if (tex.width <= 0 || tex.height <= 0) return;
    float aspect = (float)tex.width / (float)tex.height;
    e.ry = e.rx / aspect;
}

static void match_aspect_player(const Asset_Texture& tex) {
    if (tex.width <= 0 || tex.height <= 0) return;
    float aspect = (float)tex.width / (float)tex.height;
    player_ry = player_rx / aspect;
}

static bool any_overlap(const Ent& m, bool with_player) {
    if (with_player && hits_player(m)) return true;
    for (const auto& e : entities) {
        if (e.will_erase) continue;
        if (hits(m, e)) return true;
    }
    return false;
}

static bool is_target_theme(int theme) {
    if (theme < 0 || theme >= (int)target_bools.size()) return false;
    return target_bools[theme] != 0;
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
        sdl_window    = SDL_CreateWindow("Plunder", window_width, window_height, 0);
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

    tex_ship.resize(NUM_SHIP_TYPES);
    for (int i = 0; i < NUM_SHIP_TYPES; i++)
        tex_ship[i].load("assets/misc_assets/ship_" + std::to_string(i + 1) + ".png");
    tex_bullet.load("assets/misc_assets/cannonBall.png");
    tex_panel.load("assets/misc_assets/panel_wood.png");
    tex_target_bg.load("assets/misc_assets/target_red2.png");
    tex_explosion.load("assets/misc_assets/explosion1.png");

    std::vector<std::string> bg_names = {
        "assets/water_backgrounds/water1.png",
        "assets/water_backgrounds/water2.png",
        "assets/water_backgrounds/water3.png",
        "assets/water_backgrounds/water4.png",
        "assets/water_backgrounds/underwater1.png",
        "assets/water_backgrounds/underwater2.png",
        "assets/water_backgrounds/underwater3.png",
    };
    bg_textures.resize(bg_names.size());
    for (int i = 0; i < (int)bg_names.size(); i++)
        bg_textures[i].load(bg_names[i]);

    reset_game();
    return 0;
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

void reset_game() {
    entities.clear();
    cur_time = 0;
    last_fire_time = 0;
    juice_left = 1.f;
    targets_hit = 0;
    target_quota = TARGET_QUOTA;
    R_SCALE = (distribution_mode == DIST_EASY) ? 1.5f : 1.0f;
    bg_index = randn((int)bg_textures.size());

    image_permutation.resize(NUM_SHIP_TYPES);
    std::iota(image_permutation.begin(), image_permutation.end(), 0);
    std::shuffle(image_permutation.begin(), image_permutation.end(), rng);

    target_bools.assign(NUM_SHIP_TYPES, 0);
    for (int i = 0; i < NUM_CURRENT_TYPES / 2; i++)
        target_bools[image_permutation[i]] = 1;

    lane_directions.resize(num_lanes);
    lane_vels.resize(num_lanes);
    for (int i = 0; i < num_lanes; i++) {
        lane_directions[i] = rand01() < 0.5f;
        lane_vels[i] = 0.15f + 0.1f * rand01();
    }

    player_rx = R_SCALE;
    player_theme = image_permutation[randn(NUM_CURRENT_TYPES / 2) + NUM_CURRENT_TYPES / 2];
    match_aspect_player(tex_ship[player_theme]);
    player_rot = -(float)M_PI / 2.f;
    player_vx = player_vy = 0.f;
    player_x = rand01() * (WORLD_W - 2.f * player_rx) + player_rx;
    player_y = 1.f + player_ry;

    legend_r = 2.f;
    min_agent_x = 2.f * legend_r + player_rx;
    if (player_x < min_agent_x)
        player_x = min_agent_x;

    Ent bg;
    bg.type = ENT_LEGEND_BG;
    bg.x = bg.y = legend_r;
    bg.vx = bg.vy = 0;
    bg.rx = bg.ry = legend_r;
    entities.push_back(bg);

    Ent legend;
    legend.type = ENT_LEGEND;
    legend.x = legend.y = legend_r;
    legend.vx = legend.vy = 0;
    legend.rx = R_SCALE * 1.5f;
    legend.theme = image_permutation[0];
    match_aspect(legend, tex_ship[legend.theme]);
    legend.rotation = (float)M_PI / 2.f;
    entities.push_back(legend);

    int num_panels = (distribution_mode == DIST_EASY) ? 0 : randn(4);
    float panel_width = 1.2f;
    for (int i = 0; i < num_panels; i++) {
        Ent p;
        p.type = ENT_PANEL;
        p.rx = panel_width;
        p.ry = 0.5f;
        p.vx = p.vy = 0;
        bool ok = false;
        for (int attempt = 0; attempt < 80; attempt++) {
            p.x = rand01() * WORLD_W;
            p.y = 0.25f * WORLD_H + rand01() * (0.25f * WORLD_H);
            if (!any_overlap(p, true)) { ok = true; break; }
        }
        if (ok)
            entities.push_back(p);
    }
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

int32_t cenv_step(cenv_key_value* actions, int32_t actions_size) {
    int action = 4;
    for (int i = 0; i < actions_size; i++) {
        std::string key(actions[i].key);
        if (key == "action") {
            assert(actions[i].value_type == CENV_VALUE_TYPE_INT);
            action = actions[i].value_buffer.i[0];
        }
    }

    int move_action = action % 9;
    int special_action = 0;
    if (action >= 9) {
        special_action = action - 8;
        move_action = 4;
    }

    float avx = (float)(move_action / 3 - 1);

    player_vx = DECAY * ((1.f - MIXRATE) * player_vx + MIXRATE * MAXSPEED * avx);
    player_vy = 0.f;
    player_x += player_vx;
    player_y = 1.f + player_ry;
    if (player_x < min_agent_x) player_x = min_agent_x;
    if (player_x > WORLD_W - player_rx) player_x = WORLD_W - player_rx;
    if (player_x < player_rx) player_x = player_rx;

    for (auto& e : entities) {
        if (e.will_erase) continue;
        e.x += e.vx;
        e.y += e.vy;
        if (e.expire > 0) {
            e.expire--;
            if (e.expire == 0) e.will_erase = true;
        }
        if (e.type == ENT_SHIP) {
            if (e.x + e.rx < 0.f || e.x - e.rx > WORLD_W)
                e.will_erase = true;
        }
        if (e.type == ENT_BULLET) {
            if (e.y - e.ry > WORLD_H)
                e.will_erase = true;
        }
    }

    float reward = 0.f;
    bool terminated = false;
    std::vector<Ent> spawned;

    for (auto& b : entities) {
        if (b.will_erase || b.type != ENT_BULLET) continue;
        for (auto& t : entities) {
            if (t.will_erase || &t == &b) continue;
            if (!hits(b, t)) continue;
            if (t.type == ENT_SHIP) {
                t.will_erase = true;
                b.will_erase = true;
                if (is_target_theme(t.theme)) {
                    targets_hit += 1;
                    reward += POSITIVE_REWARD;
                    juice_left += 0.1f;
                } else {
                    juice_left -= 0.1f;
                }
                Ent ex;
                ex.type = ENT_EXPLOSION;
                ex.x = t.x; ex.y = t.y;
                ex.vx = t.vx * 0.5f; ex.vy = t.vy * 0.5f;
                ex.rx = ex.ry = 0.5f * t.rx;
                ex.expire = 8;
                spawned.push_back(ex);
                break;
            } else if (t.type == ENT_PANEL) {
                b.will_erase = true;
                break;
            }
        }
    }
    entities.insert(entities.end(), spawned.begin(), spawned.end());

    if (rand01() < SPAWN_PROB) {
        float ent_r = R_SCALE;
        int lane = randn(num_lanes);
        float ent_y = (lane * 0.11f + 0.4f) * (WORLD_H / 2.f - ent_r) + WORLD_H / 2.f;
        bool moves_right = lane_directions[lane] != 0;
        float ent_vx = lane_vels[lane] * (moves_right ? 1.f : -1.f);
        Ent s;
        s.type = ENT_SHIP;
        s.y = ent_y;
        s.vx = ent_vx; s.vy = 0;
        s.rx = ent_r;
        s.theme = image_permutation[randn(NUM_CURRENT_TYPES)];
        match_aspect(s, tex_ship[s.theme]);
        s.x = moves_right ? -ent_r : (WORLD_W + ent_r);
        s.reflected = !moves_right;
        s.rotation = 0.f;
        if (!any_overlap(s, false))
            entities.push_back(s);
    }

    if (special_action == 1 && (cur_time - last_fire_time) >= 3) {
        Ent b;
        b.type = ENT_BULLET;
        b.x = player_x;
        b.y = player_y;
        b.vx = 0.f;
        b.vy = 1.f;
        b.rx = b.ry = 0.25f;
        b.expire = 50;
        entities.push_back(b);
        last_fire_time = cur_time;
        juice_left -= 0.02f;
    }

    juice_left -= 0.0015f;
    if (juice_left >= 1.f) juice_left = 1.f;
    if (juice_left <= 0.f) terminated = true;

    if (targets_hit >= target_quota) {
        terminated = true;
        reward += COMPLETION_BONUS;
    }

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

static void blit(Asset_Texture* tex, float x, float y, float rx, float ry,
                 float cam_x, float cam_y, float pix, int w, int h,
                 float rotation, bool flip) {
    if (!tex || tex->width == 0) return;
    SDL_FRect dst;
    dst.w = 2.f * rx * pix;
    dst.h = 2.f * ry * pix;
    dst.x = (x - cam_x) * pix + 0.5f * (float)w - rx * pix;
    dst.y = (cam_y - y) * pix + 0.5f * (float)h - ry * pix;
    SDL_Texture* t = gr.rendering_obs ? tex->obs_texture : tex->window_texture;
    SDL_RenderTextureRotated(gr.get_renderer(), t, nullptr, &dst,
                             rotation * 180.f / (float)M_PI, nullptr,
                             flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void render_game(bool is_obs) {
    gr.rendering_obs = is_obs;
    int width  = is_obs ? obs_width  : window_width;
    int height = is_obs ? obs_height : window_height;

    SDL_SetRenderDrawColor(gr.get_renderer(), 0, 0, 0, 255);
    SDL_RenderClear(gr.get_renderer());

    float view = WORLD_H;
    float pix  = (float)height / view;
    float cam_x = 0.5f * WORLD_W;
    float cam_y = 0.5f * WORLD_H;

    if (!bg_textures.empty()) {
        Asset_Texture* bg = &bg_textures[bg_index];
        if (bg->width > 0) {
            SDL_FRect dst{ 0, 0, (float)width, (float)height };
            SDL_Texture* t = is_obs ? bg->obs_texture : bg->window_texture;
            SDL_RenderTexture(gr.get_renderer(), t, nullptr, &dst);
        }
    }

    for (const auto& e : entities) {
        if (e.type == ENT_LEGEND_BG)
            blit(&tex_target_bg, e.x, e.y, e.rx, e.ry, cam_x, cam_y, pix, width, height, 0.f, false);
        else if (e.type == ENT_PANEL)
            blit(&tex_panel, e.x, e.y, e.rx, e.ry, cam_x, cam_y, pix, width, height, 0.f, false);
        else if (e.type == ENT_SHIP)
            blit(&tex_ship[e.theme], e.x, e.y, e.rx, e.ry, cam_x, cam_y, pix, width, height, e.rotation, e.reflected);
        else if (e.type == ENT_LEGEND)
            blit(&tex_ship[e.theme], e.x, e.y, e.rx, e.ry, cam_x, cam_y, pix, width, height, e.rotation, false);
        else if (e.type == ENT_BULLET)
            blit(&tex_bullet, e.x, e.y, e.rx, e.ry, cam_x, cam_y, pix, width, height, 0.f, false);
        else if (e.type == ENT_EXPLOSION)
            blit(&tex_explosion, e.x, e.y, e.rx, e.ry, cam_x, cam_y, pix, width, height, 0.f, false);
    }

    blit(&tex_ship[player_theme], player_x, player_y, player_rx, player_ry,
         cam_x, cam_y, pix, width, height, player_rot, false);

    // HUD: juice (green) and quota progress (pink)
    SDL_FRect juice{ 0.25f * pix, 0.25f * pix, WORLD_W * juice_left * pix, 0.5f * pix };
    SDL_SetRenderDrawColor(gr.get_renderer(), 66, 245, 135, 255);
    SDL_RenderFillRect(gr.get_renderer(), &juice);
    float prog = (target_quota > 0) ? (targets_hit * 1.f / target_quota) : 0.f;
    SDL_FRect quota{ 0.25f * pix, 0.75f * pix, WORLD_W * prog * pix, 0.5f * pix };
    SDL_SetRenderDrawColor(gr.get_renderer(), 245, 66, 144, 255);
    SDL_RenderFillRect(gr.get_renderer(), &quota);

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
    tex_ship.clear();
    tex_bullet = Asset_Texture();
    tex_panel = Asset_Texture();
    tex_target_bg = Asset_Texture();
    tex_explosion = Asset_Texture();
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
