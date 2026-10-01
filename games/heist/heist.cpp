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
#include "maze_gen.h"

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

// Hard mode
const int WORLD_DIM = 13;
const float MIXRATE  = 0.5f;
const float MAXSPEED = 0.75f;
const float DECAY    = 0.9f;
const float COMPLETION_BONUS = 10.0f;
const int TIMEOUT = 1000;
const int MIN_MAZE_DIM = 5;

enum EntType { ENT_KEY = 0, ENT_DOOR, ENT_EXIT };

struct Ent {
    EntType type;
    float x, y, rx, ry;
    int theme = 0;
    bool will_erase = false;
};

Asset_Texture tex_player, tex_wall, tex_exit;
std::vector<Asset_Texture> tex_key, tex_door;
std::vector<Asset_Texture> bg_textures;

std::mt19937 rng;
SDL_Surface*  window_target = nullptr;
SDL_Surface*  obs_target    = nullptr;
SDL_Renderer* window_renderer = nullptr;
SDL_Renderer* obs_renderer    = nullptr;
uint32_t rmask, gmask, bmask, amask;

float player_x, player_y, player_vx, player_vy, player_rot, player_r, player_cr;
int world_dim = WORLD_DIM;
float maze_scale = 1.f;
int num_keys = 0;
std::vector<char> has_keys;
std::vector<int> world_grid;  // world_dim * world_dim, WALL_OBJ or SPACE
std::vector<Ent> entities;
int cur_time = 0;
int bg_index = 0;
MazeGen maze_gen;

void render_game(bool is_obs);
void reset_game();
void cenv_close();

static float rand01() { return std::uniform_real_distribution<float>(0.f, 1.f)(rng); }
static int randn(int n) {
    if (n <= 1) return 0;
    return std::uniform_int_distribution<int>(0, n - 1)(rng);
}

static int grid_at(int x, int y) {
    if (x < 0 || y < 0 || x >= world_dim || y >= world_dim) return WALL_OBJ;
    return world_grid[y * world_dim + x];
}

static bool aabb(float x1, float y1, float rx1, float ry1,
                 float x2, float y2, float rx2, float ry2) {
    const float POS_EPS = -0.001f;
    return std::fabs(x1 - x2) < (rx1 + rx2 + POS_EPS)
        && std::fabs(y1 - y2) < (ry1 + ry2 + POS_EPS);
}

static bool blocked_at(float x, float y, float rx, float ry) {
    // Original samples the four AABB corners at 0.98 of the radius, not the
    // filled rectangle. A filled test catches the inner cell of a 90° corner
    // and pinches 1-tile hallways.
    const float m = 0.98f;
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            float px = x + rx * m * (float)(2 * i - 1);
            float py = y + ry * m * (float)(2 * j - 1);
            int gx = (int)std::floor(px);
            int gy = (int)std::floor(py);
            if (grid_at(gx, gy) == WALL_OBJ)
                return true;
        }
    }
    for (const auto& e : entities) {
        if (e.will_erase || e.type != ENT_DOOR) continue;
        if ((int)has_keys.size() > e.theme && has_keys[e.theme]) continue;
        if (aabb(x, y, rx, ry, e.x, e.y, e.rx, e.ry))
            return true;
    }
    return false;
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
        sdl_window    = SDL_CreateWindow("Heist", window_width, window_height, 0);
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

    tex_player.load("assets/misc_assets/spaceAstronauts_008.png");
    tex_wall.load("assets/kenney/Ground/Dirt/dirtCenter.png");
    tex_exit.load("assets/misc_assets/gemYellow.png");
    tex_key.resize(3);
    tex_key[0].load("assets/misc_assets/keyBlue.png");
    tex_key[1].load("assets/misc_assets/keyGreen.png");
    tex_key[2].load("assets/misc_assets/keyRed.png");
    tex_door.resize(3);
    tex_door[0].load("assets/misc_assets/lock_blue.png");
    tex_door[1].load("assets/misc_assets/lock_green.png");
    tex_door[2].load("assets/misc_assets/lock_red.png");

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

void reset_game() {
    entities.clear();
    cur_time = 0;
    player_vx = player_vy = 0;
    player_rot = 0;
    bg_index = randn((int)bg_textures.size());

    world_dim = WORLD_DIM;
    maze_scale = (float)world_dim / (float)world_dim;  // 1.0
    int max_diff = (world_dim - MIN_MAZE_DIM) / 2;
    int difficulty = randn(max_diff + 1);
    num_keys = difficulty + randn(2);
    if (num_keys > 3) num_keys = 3;
    has_keys.assign(num_keys, 0);

    int maze_dim = difficulty * 2 + MIN_MAZE_DIM;
    player_r = 0.375f * maze_scale;
    player_cr = 0.32f * maze_scale;
    float r_ent = 0.45f * maze_scale;
    float r_item = 0.375f * maze_scale;

    maze_gen.generate_maze_with_doors(maze_dim, num_keys, rng);

    world_grid.assign(world_dim * world_dim, WALL_OBJ);
    int off_x = randn(world_dim - maze_dim + 1);
    int off_y = randn(world_dim - maze_dim + 1);

    player_x = (off_x + 0.5f) * maze_scale;
    player_y = (off_y + 0.5f) * maze_scale;

    for (int i = 0; i < maze_dim; i++) {
        for (int j = 0; j < maze_dim; j++) {
            int x = off_x + i;
            int y = off_y + j;
            int obj = maze_gen.get(i + MAZE_OFFSET, j + MAZE_OFFSET);
            float obj_x = (x + 0.5f) * maze_scale;
            float obj_y = (y + 0.5f) * maze_scale;

            if (obj != WALL_OBJ)
                world_grid[y * world_dim + x] = SPACE;

            if (obj >= KEY_OBJ) {
                Ent e{ ENT_KEY, obj_x, obj_y, r_item, r_item, obj - KEY_OBJ - 1, false };
                entities.push_back(e);
            } else if (obj >= DOOR_OBJ) {
                Ent e{ ENT_DOOR, obj_x, obj_y, r_ent, r_ent, obj - DOOR_OBJ - 1, false };
                entities.push_back(e);
            } else if (obj == EXIT_OBJ) {
                Ent e{ ENT_EXIT, obj_x, obj_y, r_item, r_item, 0, false };
                entities.push_back(e);
            } else if (obj == AGENT_OBJ) {
                player_x = obj_x;
                player_y = obj_y;
            }
        }
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
    if (action >= 9) move_action = 4;
    float avx = (float)(move_action / 3 - 1);
    float avy = (float)(move_action % 3 - 1);

    player_vx = (1.f - MIXRATE) * player_vx + MIXRATE * MAXSPEED * avx;
    player_vy = (1.f - MIXRATE) * player_vy + MIXRATE * MAXSPEED * avy;
    player_vx *= DECAY;
    player_vy *= DECAY;

    if (avx != 0.f || avy != 0.f)
        player_rot = -std::atan2(avy, avx);

    float nx = player_x + player_vx;
    if (!blocked_at(nx, player_y, player_cr, player_cr)) player_x = nx;
    else player_vx = 0;
    float ny = player_y + player_vy;
    if (!blocked_at(player_x, ny, player_cr, player_cr)) player_y = ny;
    else player_vy = 0;

    float reward = 0.f;
    bool terminated = false;

    for (auto& e : entities) {
        if (e.will_erase) continue;
        if (!aabb(player_x, player_y, player_r, player_r, e.x, e.y, e.rx, e.ry))
            continue;
        if (e.type == ENT_EXIT) {
            reward = COMPLETION_BONUS;
            terminated = true;
        } else if (e.type == ENT_KEY) {
            e.will_erase = true;
            if (e.theme >= 0 && e.theme < (int)has_keys.size())
                has_keys[e.theme] = 1;
        } else if (e.type == ENT_DOOR) {
            if (e.theme >= 0 && e.theme < (int)has_keys.size() && has_keys[e.theme])
                e.will_erase = true;
        }
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

    float view = (float)world_dim;
    float pix  = (float)height / view;
    float cam_x = 0.5f * (float)world_dim;
    float cam_y = 0.5f * (float)world_dim;

    if (!bg_textures.empty()) {
        Asset_Texture* bg = &bg_textures[bg_index];
        if (bg->width > 0) {
            SDL_FRect dst{ 0, 0, (float)width, (float)height };
            SDL_Texture* t = is_obs ? bg->obs_texture : bg->window_texture;
            SDL_RenderTexture(gr.get_renderer(), t, nullptr, &dst);
        }
    }

    float wr = 0.5f * maze_scale;
    for (int gx = 0; gx < world_dim; gx++) {
        for (int gy = 0; gy < world_dim; gy++) {
            if (grid_at(gx, gy) != WALL_OBJ) continue;
            float x = (gx + 0.5f) * maze_scale;
            float y = (gy + 0.5f) * maze_scale;
            blit(&tex_wall, x, y, wr, wr, cam_x, cam_y, pix, width, height, 0.f);
        }
    }

    for (const auto& e : entities) {
        Asset_Texture* tex = nullptr;
        if (e.type == ENT_KEY && e.theme >= 0 && e.theme < (int)tex_key.size())
            tex = &tex_key[e.theme];
        else if (e.type == ENT_DOOR && e.theme >= 0 && e.theme < (int)tex_door.size())
            tex = &tex_door[e.theme];
        else if (e.type == ENT_EXIT)
            tex = &tex_exit;
        if (tex)
            blit(tex, e.x, e.y, e.rx, e.ry, cam_x, cam_y, pix, width, height, 0.f);
    }

    blit(&tex_player, player_x, player_y, player_r, player_r,
         cam_x, cam_y, pix, width, height, player_rot);

    // Collected keys HUD (top-right, original KEY_ON_RING)
    float hud_r = 0.03f * (float)world_dim;
    for (int i = 0; i < num_keys; i++) {
        if (!has_keys[i] || i >= (int)tex_key.size()) continue;
        float hx = (float)world_dim - hud_r * (2.f * i + 1.25f);
        float hy = (float)world_dim - hud_r * 1.5f;
        blit(&tex_key[i], hx, hy, hud_r, hud_r, cam_x, cam_y, pix, width, height,
             (float)M_PI / 2.f);
    }

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
    tex_wall = Asset_Texture();
    tex_exit = Asset_Texture();
    tex_key.clear();
    tex_door.clear();
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
