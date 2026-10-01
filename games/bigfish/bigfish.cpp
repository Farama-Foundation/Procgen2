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

const float WORLD_W = 20.0f;
const float WORLD_H = 20.0f;

// BasicAbstractGame defaults (BigFish does not override these)
const float MIXRATE  = 0.5f;
const float MAXSPEED = 0.5f;
const float DECAY    = 0.9f;

const float FISH_MIN_R = 0.25f;
const float FISH_MAX_R = 2.0f;
const int   FISH_QUOTA = 30;
const float START_R    = 0.5f;   // hard mode

const float COMPLETION_BONUS = 10.0f;
const float POSITIVE_REWARD  =  1.0f;
const int   TIMEOUT = 6000;

struct Fish {
    float x, y, vx, vy, r;
    int theme = 0;
    bool flip = false;
    bool will_erase = false;
};

Asset_Texture tex_player;
std::vector<Asset_Texture> tex_fish;
std::vector<Asset_Texture> bg_textures;

std::mt19937 rng;

SDL_Surface*  window_target = nullptr;
SDL_Surface*  obs_target    = nullptr;
SDL_Renderer* window_renderer = nullptr;
SDL_Renderer* obs_renderer    = nullptr;

uint32_t rmask, gmask, bmask, amask;

float player_x, player_y;
float player_vx, player_vy;
float player_r;
bool  player_flip = false;
int   fish_eaten = 0;
float r_inc = 0.f;
int   cur_time = 0;
int   bg_index = 0;

std::vector<Fish> fishes;

void render_game(bool is_obs);
void reset_game();
void cenv_close();

static float rand01() {
    return std::uniform_real_distribution<float>(0.f, 1.f)(rng);
}
static int randn(int n) {
    return std::uniform_int_distribution<int>(0, n - 1)(rng);
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
        sdl_window    = SDL_CreateWindow("BigFish", window_width, window_height, 0);
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

    tex_player.load("assets/misc_assets/fishTile_072.png");
    tex_fish.resize(3);
    tex_fish[0].load("assets/misc_assets/fishTile_074.png");
    tex_fish[1].load("assets/misc_assets/fishTile_078.png");
    tex_fish[2].load("assets/misc_assets/fishTile_080.png");

    std::vector<std::string> bg_names = {
        "assets/water_backgrounds/underwater1.png",
        "assets/water_backgrounds/underwater2.png",
        "assets/water_backgrounds/underwater3.png",
        "assets/water_backgrounds/water1.png",
        "assets/water_backgrounds/water2.png",
        "assets/water_backgrounds/water3.png",
        "assets/water_backgrounds/water4.png",
    };
    bg_textures.resize(bg_names.size());
    for (int i = 0; i < (int)bg_names.size(); i++)
        bg_textures[i].load(bg_names[i]);

    reset_game();
    return 0;
}

void reset_game() {
    fishes.clear();
    cur_time = 0;
    fish_eaten = 0;
    player_r = START_R;
    r_inc = (FISH_MAX_R - START_R) / (float)FISH_QUOTA;
    player_vx = 0;
    player_vy = 0;
    player_flip = false;
    player_x = rand01() * (WORLD_W - 2.f * player_r) + player_r;
    player_y = 1.f + player_r;
    bg_index = randn((int)bg_textures.size());
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

static bool aabb(float x1, float y1, float r1, float x2, float y2, float r2) {
    return std::fabs(x1 - x2) < (r1 + r2) && std::fabs(y1 - y2) < (r1 + r2);
}

static bool out_of_bounds(float x, float y, float r) {
    return (x + r < 0.f) || (y + r < 0.f)
        || (x - r > WORLD_W) || (y - r > WORLD_H);
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
    if (action >= 9)
        move_action = 4;

    float avx = (float)(move_action / 3 - 1);
    float avy = (float)(move_action % 3 - 1);

    player_vx = (1.f - MIXRATE) * player_vx + MIXRATE * MAXSPEED * avx;
    player_vy = (1.f - MIXRATE) * player_vy + MIXRATE * MAXSPEED * avy;
    player_vx *= DECAY;
    player_vy *= DECAY;

    player_x += player_vx;
    player_y += player_vy;

    if (avx > 0.f) player_flip = false;
    if (avx < 0.f) player_flip = true;

    float reward = 0.f;
    bool terminated = false;
    bool truncated = false;

    if (out_of_bounds(player_x, player_y, player_r))
        terminated = true;

    for (auto& f : fishes) {
        if (f.will_erase) continue;
        f.x += f.vx;
        f.y += f.vy;
        if (out_of_bounds(f.x, f.y, f.r)) {
            f.will_erase = true;
            continue;
        }
        if (terminated) continue;
        if (!aabb(player_x, player_y, player_r, f.x, f.y, f.r))
            continue;
        if (f.r > player_r) {
            terminated = true;
        } else {
            reward += POSITIVE_REWARD;
            f.will_erase = true;
            player_r += r_inc;
            fish_eaten += 1;
        }
    }

    fishes.erase(std::remove_if(fishes.begin(), fishes.end(),
        [](const Fish& f){ return f.will_erase; }), fishes.end());

    // Spawn after collisions (original game_step order)
    if (randn(10) == 1) {
        Fish f;
        f.r = (FISH_MAX_R - FISH_MIN_R) * std::pow(rand01(), 1.4f) + FISH_MIN_R;
        f.y = rand01() * (WORLD_H - 2.f * f.r);
        bool moves_right = rand01() < 0.5f;
        f.vx = (0.15f + rand01() * 0.25f) * (moves_right ? 1.f : -1.f);
        f.vy = 0.f;
        f.x = moves_right ? -f.r : WORLD_W + f.r;
        f.theme = randn((int)tex_fish.size());
        f.flip = !moves_right;
        fishes.push_back(f);
    }

    if (!terminated && fish_eaten >= FISH_QUOTA) {
        reward += COMPLETION_BONUS;
        terminated = true;
    }

    cur_time++;
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

static void blit(Asset_Texture* tex, float x, float y, float r,
                 float cam_x, float cam_y, float pix, int w, int h, bool flip) {
    if (!tex || tex->width == 0) return;
    SDL_FRect dst;
    dst.w = 2.f * r * pix;
    dst.h = 2.f * r * pix;
    dst.x = (x - cam_x) * pix + 0.5f * (float)w - r * pix;
    dst.y = (cam_y - y) * pix + 0.5f * (float)h - r * pix;
    SDL_Texture* t = gr.rendering_obs ? tex->obs_texture : tex->window_texture;
    SDL_RenderTextureRotated(gr.get_renderer(), t, nullptr, &dst, 0.0,
                             nullptr, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

void render_game(bool is_obs) {
    gr.rendering_obs = is_obs;
    int width  = is_obs ? obs_width  : window_width;
    int height = is_obs ? obs_height : window_height;

    SDL_SetRenderDrawColor(gr.get_renderer(), 0, 0, 0, 255);
    SDL_RenderClear(gr.get_renderer());

    float view = WORLD_W;
    float pix  = (float)height / view;
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

    for (const auto& f : fishes) {
        blit(&tex_fish[f.theme % (int)tex_fish.size()],
             f.x, f.y, f.r, cam_x, cam_y, pix, width, height, f.flip);
    }

    blit(&tex_player, player_x, player_y, player_r,
         cam_x, cam_y, pix, width, height, player_flip);

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

    fishes.clear();
    tex_player = Asset_Texture();
    tex_fish.clear();
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
