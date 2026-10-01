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

// ---------------------- CEnv Interface ----------------------

cenv_make_data make_data;
cenv_reset_data reset_data;
cenv_step_data step_data;
cenv_render_data render_data;

cenv_key_value observation;

static bool g_initialized = false;
static bool g_human_mode  = false;
static SDL_Window* sdl_window = nullptr;

// ---------------------- Game Constants ----------------------

const int obs_width  = 64;
const int obs_height = 64;
const int num_actions = 15;

int window_width  = 512;
int window_height = 512;

// Hard-mode defaults (original FruitBot)
const float WORLD_H = 60.0f;
const float MIXRATE  = 0.5f;
const float MAXSPEED = 0.85f;
const float DECAY    = 0.9f;
const float AUTO_VY  = 0.2f;   // constant upward intent (original set_action_xy)

const float COMPLETION_BONUS = 10.0f;
const float POSITIVE_REWARD  =  1.0f;
const float PENALTY          = -4.0f;

const int KEY_DURATION = 8;
const float BULLET_VSCALE = 0.5f;
const float DOOR_ASPECT   = 3.25f;

const int TIMEOUT = 1000;

const float WALL_RY = 0.3f;
const float LOCK_RX = 0.25f;
const float LOCK_RY = 0.45f;
const float PLAYER_R = 0.4f;

// ---------------------- Entity Types ----------------------

enum ObjType {
    OBJ_DEAD = 0,
    OBJ_BARRIER,
    OBJ_GOOD,       // fruit  +1
    OBJ_BAD,        // food   -4
    OBJ_DOOR,
    OBJ_LOCK,
    OBJ_PRESENT,
    OBJ_KEY         // player bullet
};

struct Obj {
    ObjType type = OBJ_DEAD;
    float x = 0, y = 0;
    float vx = 0, vy = 0;
    float rx = 0.5f, ry = 0.5f;
    int theme = 0;
    int life  = 0;          // key expire countdown
    bool will_erase = false;
};

// ---------------------- Textures ----------------------

Asset_Texture tex_player;
Asset_Texture tex_barrier;
Asset_Texture tex_key;
Asset_Texture tex_door;
Asset_Texture tex_lock;
std::vector<Asset_Texture> tex_good;
std::vector<Asset_Texture> tex_bad;
std::vector<Asset_Texture> tex_present;
std::vector<Asset_Texture> bg_textures;

// ---------------------- Game State ----------------------

std::mt19937 rng;

SDL_Surface*  window_target = nullptr;
SDL_Surface*  obs_target    = nullptr;
SDL_Renderer* window_renderer = nullptr;
SDL_Renderer* obs_renderer    = nullptr;

uint32_t rmask, gmask, bmask, amask;

float world_w = 20.0f;
float world_h = WORLD_H;

float player_x, player_y;
float player_vx, player_vy;
bool  player_alive;

std::vector<Obj> entities;
int cur_time = 0;
int last_fire_time = 0;
int bg_index = 0;

// ---------------------- Forward declarations ----------------------

void render_game(bool is_obs);
void reset_game();
void cenv_close();

// ---------------------- Helpers ----------------------

static float rand01() {
    return std::uniform_real_distribution<float>(0.f, 1.f)(rng);
}
static int randn(int n) {  // [0, n)
    return std::uniform_int_distribution<int>(0, n - 1)(rng);
}
static float rand_pos(float r, float lo, float hi) {
    if (hi - lo <= 2.f * r) return 0.5f * (lo + hi);
    return (hi - lo - 2.f * r) * rand01() + r + lo;
}

static bool aabb(const Obj& a, float x, float y, float rx, float ry, float margin = 0.f) {
    return std::fabs(a.x - x) < (a.rx + rx + margin)
        && std::fabs(a.y - y) < (a.ry + ry + margin);
}
static bool aabb(const Obj& a, const Obj& b, float margin = 0.f) {
    return aabb(a, b.x, b.y, b.rx, b.ry, margin);
}

static std::vector<int> partition(int leftover, int n) {
    std::vector<int> parts(n, 0);
    for (int i = 0; i < leftover; i++)
        parts[randn(n)] += 1;
    return parts;
}

// ---------------------- Level generation ----------------------

static void add_walls(float wy, bool use_door, float min_pct) {
    float rw = world_w;
    float pct = min_pct + 0.2f * rand01();

    if (use_door) {
        pct += 0.1f;
        float lock_pct_w = 2.f * LOCK_RX / rw;
        float door_pct_w = (WALL_RY * 2.f * DOOR_ASPECT) / rw;
        int num_doors = (int)std::ceil((pct - 2.f * lock_pct_w) / door_pct_w);
        pct = 2.f * lock_pct_w + door_pct_w * (float)num_doors;
    }

    float gapw = pct * rw;
    float w1 = rand01() * (rw - gapw);
    float w2 = rw - w1 - gapw;

    Obj left;
    left.type = OBJ_BARRIER;
    left.x = w1 * 0.5f; left.y = wy;
    left.rx = w1 * 0.5f; left.ry = WALL_RY;
    entities.push_back(left);

    Obj right;
    right.type = OBJ_BARRIER;
    right.x = rw - w2 * 0.5f; right.y = wy;
    right.rx = w2 * 0.5f; right.ry = WALL_RY;
    entities.push_back(right);

    if (use_door) {
        int is_on_right = randn(2);
        float lock_x = w1 + LOCK_RX + (float)is_on_right * (gapw - 2.f * LOCK_RX);
        float door_x = w1 + gapw * 0.5f - (float)(is_on_right * 2 - 1) * LOCK_RX;

        Obj door;
        door.type = OBJ_DOOR;
        door.x = door_x; door.y = wy;
        door.rx = gapw * 0.5f - LOCK_RX; door.ry = WALL_RY;
        entities.push_back(door);

        Obj lock;
        lock.type = OBJ_LOCK;
        lock.x = lock_x; lock.y = wy - LOCK_RY + WALL_RY;
        lock.rx = LOCK_RX; lock.ry = LOCK_RY;
        entities.push_back(lock);
    }
}

static bool overlaps_any(float x, float y, float r) {
    Obj probe; probe.x = x; probe.y = y; probe.rx = r; probe.ry = r;
    if (aabb(probe, player_x, player_y, PLAYER_R, PLAYER_R))
        return true;
    for (const auto& e : entities) {
        if (e.will_erase) continue;
        if (aabb(probe, e))
            return true;
    }
    return false;
}

static void spawn_scattered(int count, ObjType type, int nthemes) {
    for (int i = 0; i < count; i++) {
        Obj o;
        o.type = type;
        o.rx = 0.5f; o.ry = 0.5f;
        o.theme = randn(nthemes);
        int tries = 0;
        do {
            o.x = rand_pos(o.rx, 0.f, world_w);
            o.y = rand_pos(o.ry, 0.f, world_h);
            tries++;
        } while (overlaps_any(o.x, o.y, o.rx) && tries < 100);
        entities.push_back(o);
    }
}

// ---------------------- CEnv Interface Implementation ----------------------

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
        sdl_window    = SDL_CreateWindow("FruitBot", window_width, window_height, 0);
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

    tex_player.load("assets/misc_assets/robot_3Dblue.png");
    tex_barrier.load("assets/misc_assets/tileStone_slope.png");
    tex_key.load("assets/misc_assets/keyRed2.png");
    tex_door.load("assets/misc_assets/fenceYellow.png");
    tex_lock.load("assets/misc_assets/lockRed2.png");

    tex_good.resize(6);
    tex_bad.resize(6);
    for (int i = 0; i < 6; i++) {
        tex_good[i].load("assets/misc_assets/fruit" + std::to_string(i + 1) + ".png");
        tex_bad[i].load("assets/misc_assets/food"  + std::to_string(i + 1) + ".png");
    }
    tex_present.resize(3);
    for (int i = 0; i < 3; i++)
        tex_present[i].load("assets/misc_assets/present" + std::to_string(i + 1) + ".png");

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
    last_fire_time = -KEY_DURATION;
    player_alive = true;
    player_vx = 0;
    player_vy = 0;

    bg_index = randn((int)bg_textures.size());

    // Hard mode (original defaults)
    world_w = 20.f;
    world_h = WORLD_H;

    const int min_sep = 4;
    const int num_walls = 10;
    const int object_group_size = 6;
    const int buf_h = 4;
    const float door_prob = 0.125f;
    const float min_pct = 0.1f;

    int leftover = (int)world_h - min_sep * num_walls - buf_h;
    if (leftover < 0) leftover = 0;
    std::vector<int> parts = partition(leftover, num_walls);

    int curr_h = 0;
    for (int part : parts) {
        int dy = min_sep + part;
        curr_h += dy;
        bool use_door = (dy > 5) && (rand01() < door_prob);
        add_walls((float)curr_h, use_door, min_pct);
    }

    player_x = rand_pos(PLAYER_R, 0.f, world_w);
    player_y = PLAYER_R;

    for (int i = 0; i < (int)world_w; i++) {
        Obj p;
        p.type = OBJ_PRESENT;
        p.x = (float)i + 0.5f;
        p.y = world_h - 0.5f;
        p.rx = 0.5f; p.ry = 0.5f;
        p.theme = randn((int)tex_present.size());
        entities.push_back(p);
    }

    int num_good = randn(10) + 10;
    int num_bad  = randn(10) + 10;
    spawn_scattered(num_good, OBJ_GOOD, object_group_size);
    spawn_scattered(num_bad,  OBJ_BAD,  object_group_size);
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
    int special_action = 0;
    if (action >= 9) {
        special_action = action - 8;
        move_action = 4;
    }

    float avx = (float)(move_action / 3 - 1);
    float avy = AUTO_VY;

    player_vx = (1.f - MIXRATE) * player_vx;
    player_vy = (1.f - MIXRATE) * player_vy;
    player_vx += MIXRATE * MAXSPEED * avx;
    player_vy += MIXRATE * MAXSPEED * avy;
    player_vx *= DECAY;
    player_vy *= DECAY;

    player_x += player_vx;
    player_y += player_vy;

    // Stay inside the corridor (OUT_OF_BOUNDS_WALL)
    if (player_x < PLAYER_R) { player_x = PLAYER_R; player_vx = 0; }
    if (player_x > world_w - PLAYER_R) { player_x = world_w - PLAYER_R; player_vx = 0; }
    if (player_y < PLAYER_R) { player_y = PLAYER_R; player_vy = 0; }
    if (player_y > world_h - PLAYER_R) { player_y = world_h - PLAYER_R; player_vy = 0; }

    if (special_action == 1 && (cur_time - last_fire_time) >= KEY_DURATION) {
        Obj key;
        key.type = OBJ_KEY;
        key.x = player_x; key.y = player_y;
        key.vx = 0; key.vy = BULLET_VSCALE;
        key.rx = 0.25f; key.ry = 0.25f;
        key.life = KEY_DURATION;
        entities.push_back(key);
        last_fire_time = cur_time;
    }

    cur_time++;

    float reward = 0.f;
    bool terminated = false;
    bool truncated = false;

    // Move keys, then resolve collisions
    for (auto& e : entities) {
        if (e.will_erase) continue;
        if (e.type == OBJ_KEY) {
            e.x += e.vx;
            e.y += e.vy;
            e.life--;
            if (e.life <= 0) e.will_erase = true;
        }
    }

    for (auto& e : entities) {
        if (e.will_erase) continue;

        if (e.type == OBJ_KEY) {
            for (auto& t : entities) {
                if (t.will_erase || &t == &e) continue;
                if (!aabb(e, t)) continue;
                if (t.type == OBJ_BARRIER) {
                    e.will_erase = true;
                    break;
                }
                if (t.type == OBJ_LOCK) {
                    e.will_erase = true;
                    t.will_erase = true;
                    for (auto& d : entities) {
                        if (d.type == OBJ_DOOR && !d.will_erase && std::fabs(d.y - t.y) < 1.f)
                            d.will_erase = true;
                    }
                    break;
                }
            }
            continue;
        }

        if (!aabb(e, player_x, player_y, PLAYER_R, PLAYER_R))
            continue;

        if (e.type == OBJ_BARRIER || e.type == OBJ_DOOR) {
            player_alive = false;
            terminated = true;
            break;
        }
        if (e.type == OBJ_GOOD) {
            reward += POSITIVE_REWARD;
            e.will_erase = true;
        } else if (e.type == OBJ_BAD) {
            reward += PENALTY;
            e.will_erase = true;
        } else if (e.type == OBJ_PRESENT) {
            reward += COMPLETION_BONUS;
            terminated = true;
            e.will_erase = true;
        }
    }

    entities.erase(std::remove_if(entities.begin(), entities.end(),
        [](const Obj& o){ return o.will_erase; }), entities.end());

    if (!terminated && cur_time >= TIMEOUT)
        truncated = true;

    step_data.reward.f   = reward;
    step_data.terminated = terminated || !player_alive;
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

// ---------------------- Rendering ----------------------

// Y-up world: larger y is toward the top of the level / top of the screen.
static void blit(Asset_Texture* tex, float x, float y, float rx, float ry,
                 float cam_x, float cam_y, float pix, int w, int h) {
    if (!tex || tex->width == 0) return;
    SDL_FRect dst;
    dst.w = 2.f * rx * pix;
    dst.h = 2.f * ry * pix;
    dst.x = (x - cam_x) * pix + 0.5f * (float)w - rx * pix;
    dst.y = (cam_y - y) * pix + 0.5f * (float)h - ry * pix;
    SDL_Texture* t = gr.rendering_obs ? tex->obs_texture : tex->window_texture;
    SDL_RenderTexture(gr.get_renderer(), t, nullptr, &dst);
}

void render_game(bool is_obs) {
    gr.rendering_obs = is_obs;
    int width  = is_obs ? obs_width  : window_width;
    int height = is_obs ? obs_height : window_height;

    SDL_SetRenderDrawColor(gr.get_renderer(), 0, 0, 0, 255);
    SDL_RenderClear(gr.get_renderer());

    // Visibility is the corridor width (original choose_center / visibility)
    float view = world_w;
    float pix  = (float)height / view;

    float cam_x = world_w * 0.5f;
    float cam_y = player_y + world_w * 0.5f - 2.f * PLAYER_R;

    // Background
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
            case OBJ_BARRIER: tex = &tex_barrier; break;
            case OBJ_GOOD:    tex = &tex_good[e.theme % (int)tex_good.size()]; break;
            case OBJ_BAD:     tex = &tex_bad[e.theme % (int)tex_bad.size()]; break;
            case OBJ_DOOR:    tex = &tex_door; break;
            case OBJ_LOCK:    tex = &tex_lock; break;
            case OBJ_PRESENT: tex = &tex_present[e.theme % (int)tex_present.size()]; break;
            case OBJ_KEY:     tex = &tex_key; break;
            default: break;
        }
        if (tex) blit(tex, e.x, e.y, e.rx, e.ry, cam_x, cam_y, pix, width, height);
    }

    if (player_alive)
        blit(&tex_player, player_x, player_y, PLAYER_R, PLAYER_R, cam_x, cam_y, pix, width, height);

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

    tex_player  = Asset_Texture();
    tex_barrier = Asset_Texture();
    tex_key     = Asset_Texture();
    tex_door    = Asset_Texture();
    tex_lock    = Asset_Texture();
    tex_good.clear();
    tex_bad.clear();
    tex_present.clear();
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
