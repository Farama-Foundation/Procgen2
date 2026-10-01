#include "../../cenv/cenv.h"

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
const int WORLD_W = 20;
const int WORLD_H = 20;
const float COMPLETION_BONUS = 10.f;
const float DIAMOND_REWARD   = 1.f;
const int TIMEOUT = 1000;

enum Cell {
    CELL_SPACE = 0,
    CELL_DIRT,
    CELL_BOULDER,
    CELL_MOVING_BOULDER,
    CELL_DIAMOND,
    CELL_MOVING_DIAMOND,
    CELL_OOB
};

Asset_Texture tex_player, tex_boulder, tex_diamond, tex_exit, tex_dirt, tex_brick;
std::vector<Asset_Texture> bg_textures;

std::mt19937 rng;
SDL_Surface*  window_target = nullptr;
SDL_Surface*  obs_target    = nullptr;
SDL_Renderer* window_renderer = nullptr;
SDL_Renderer* obs_renderer    = nullptr;
uint32_t rmask, gmask, bmask, amask;

int agent_x = 0, agent_y = 0;  // cell coords
int agent_vx = 0;              // last attempted horizontal dir (for push)
bool player_flip = false;
int diamonds_remaining = 0;
int exit_x = 0, exit_y = 0;
int cur_time = 0;
int bg_index = 0;
std::vector<int> grid;  // WORLD_W * WORLD_H

void render_game(bool is_obs);
void reset_game();
void cenv_close();

static int randn(int n) {
    if (n <= 1) return 0;
    return std::uniform_int_distribution<int>(0, n - 1)(rng);
}

static int idx(int x, int y) { return y * WORLD_W + x; }

static int get_obj(int x, int y) {
    if (x < 0 || y < 0 || x >= WORLD_W || y >= WORLD_H) return CELL_OOB;
    return grid[idx(x, y)];
}
static int get_obj_i(int i) {
    if (i < 0 || i >= WORLD_W * WORLD_H) return CELL_OOB;
    return grid[i];
}
static void set_obj(int x, int y, int v) {
    if (x < 0 || y < 0 || x >= WORLD_W || y >= WORLD_H) return;
    grid[idx(x, y)] = v;
}
static void set_obj_i(int i, int v) {
    if (i < 0 || i >= WORLD_W * WORLD_H) return;
    grid[i] = v;
}

static int stationary(int t) {
    if (t == CELL_MOVING_DIAMOND) return CELL_DIAMOND;
    if (t == CELL_MOVING_BOULDER) return CELL_BOULDER;
    return t;
}
static int moving(int t) {
    if (t == CELL_DIAMOND) return CELL_MOVING_DIAMOND;
    if (t == CELL_BOULDER) return CELL_MOVING_BOULDER;
    return t;
}
static bool is_moving(int t) {
    return t == CELL_MOVING_BOULDER || t == CELL_MOVING_DIAMOND;
}
static bool is_round(int t) {
    t = stationary(t);
    return t == CELL_BOULDER || t == CELL_DIAMOND;
}
static bool is_free(int x, int y) {
    return get_obj(x, y) == CELL_SPACE && !(x == agent_x && y == agent_y);
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
        sdl_window    = SDL_CreateWindow("Miner", window_width, window_height, 0);
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

    tex_player.load("assets/misc_assets/robot_greenDrive1.png");
    tex_boulder.load("assets/misc_assets/elementStone007.png");
    tex_diamond.load("assets/misc_assets/gemBlue.png");
    tex_exit.load("assets/misc_assets/window.png");
    tex_dirt.load("assets/misc_assets/dirt.png");
    tex_brick.load("assets/misc_assets/tile_bricksGrey.png");

    std::vector<std::string> bg_names = {
        "assets/platform_backgrounds/back_cave.png",
        "assets/platform_backgrounds/airadventurelevel1.png",
        "assets/platform_backgrounds/airadventurelevel2.png",
        "assets/platform_backgrounds/alien_bg.png",
        "assets/platform_backgrounds/another_world_bg.png",
        "assets/platform_backgrounds/battleback1.png",
        "assets/platform_backgrounds/battleback2.png",
        "assets/platform_backgrounds/battleback3.png",
    };
    bg_textures.resize(bg_names.size());
    for (int i = 0; i < (int)bg_names.size(); i++)
        bg_textures[i].load(bg_names[i]);

    reset_game();
    return 0;
}

void reset_game() {
    cur_time = 0;
    player_flip = false;
    agent_vx = 0;
    bg_index = randn((int)bg_textures.size());

    const int area = WORLD_W * WORLD_H;
    grid.assign(area, CELL_DIRT);

    int num_diamonds = (int)(12.f / 400.f * area);  // 12
    int num_boulders = (int)(80.f / 400.f * area);  // 80
    int n_pick = num_diamonds + num_boulders + 1;

    std::vector<int> cells(area);
    std::iota(cells.begin(), cells.end(), 0);
    for (int i = area - 1; i > 0; i--)
        std::swap(cells[i], cells[randn(i + 1)]);

    agent_x = cells[0] % WORLD_W;
    agent_y = cells[0] / WORLD_W;

    for (int i = 0; i < num_diamonds; i++) {
        int cell = cells[i + 1];
        grid[cell] = CELL_DIAMOND;
    }
    for (int i = 0; i < num_boulders; i++) {
        int cell = cells[i + 1 + num_diamonds];
        grid[cell] = CELL_BOULDER;
    }

    std::vector<int> dirt_cells;
    for (int i = 0; i < area; i++)
        if (grid[i] == CELL_DIRT) dirt_cells.push_back(i);

    set_obj(agent_x, agent_y, CELL_SPACE);
    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++)
            if (get_obj(agent_x + i, agent_y + j) == CELL_BOULDER)
                set_obj(agent_x + i, agent_y + j, CELL_DIRT);

    std::vector<int> exit_cands;
    for (int cell : dirt_cells) {
        int x = cell % WORLD_W, y = cell / WORLD_W;
        int above = get_obj(x, y + 1);
        if (above == CELL_DIRT || above == CELL_OOB)
            exit_cands.push_back(cell);
    }
    if (exit_cands.empty()) {
        exit_x = 0; exit_y = WORLD_H - 1;
        set_obj(exit_x, exit_y, CELL_SPACE);
    } else {
        int exit_cell = exit_cands[randn((int)exit_cands.size())];
        exit_x = exit_cell % WORLD_W;
        exit_y = exit_cell / WORLD_W;
        set_obj(exit_x, exit_y, CELL_SPACE);
    }

    diamonds_remaining = num_diamonds;
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
    int dx = move_action / 3 - 1;
    int dy = move_action % 3 - 1;
    if (dx != 0) dy = 0;  // original set_action_xy: no diagonals

    agent_vx = dx;
    if (dx > 0) player_flip = false;
    if (dx < 0) player_flip = true;

    float reward = 0.f;
    bool terminated = false;

    int moved = 0;
    if (dx != 0 || dy != 0) {
        int nx = agent_x + dx;
        int ny = agent_y + dy;
        int tgt = get_obj(nx, ny);
        if (tgt == CELL_OOB || tgt == CELL_BOULDER || tgt == CELL_MOVING_BOULDER) {
            // blocked — handle_push may still slide a boulder
        } else {
            agent_x = nx;
            agent_y = ny;
            moved = 1;
        }
    }

    // Push (original: after a blocked step, vx==0)
    if (!moved && dx == 1 && agent_x < WORLD_W - 2
        && get_obj(agent_x + 1, agent_y) == CELL_BOULDER
        && get_obj(agent_x + 2, agent_y) == CELL_SPACE) {
        set_obj(agent_x + 1, agent_y, CELL_SPACE);
        set_obj(agent_x + 2, agent_y, CELL_BOULDER);
        agent_x += 1;
        moved = 1;
    } else if (!moved && dx == -1 && agent_x > 1
        && get_obj(agent_x - 1, agent_y) == CELL_BOULDER
        && get_obj(agent_x - 2, agent_y) == CELL_SPACE) {
        set_obj(agent_x - 1, agent_y, CELL_SPACE);
        set_obj(agent_x - 2, agent_y, CELL_BOULDER);
        agent_x -= 1;
        moved = 1;
    }

    int standing = get_obj(agent_x, agent_y);
    if (standing == CELL_DIAMOND)
        reward += DIAMOND_REWARD;
    if (standing == CELL_DIRT || standing == CELL_DIAMOND)
        set_obj(agent_x, agent_y, CELL_SPACE);

    const int area = WORLD_W * WORLD_H;
    int diamonds_count = 0;
    for (int i = 0; i < area; i++) {
        int obj = get_obj_i(i);
        int obj_x = i % WORLD_W;
        int obj_y = i / WORLD_W;
        int stat = stationary(obj);
        if (stat == CELL_DIAMOND) diamonds_count++;

        if (obj == CELL_BOULDER || obj == CELL_MOVING_BOULDER
            || obj == CELL_DIAMOND || obj == CELL_MOVING_DIAMOND) {
            int below_x = obj_x, below_y = obj_y - 1;
            int obj2 = get_obj(below_x, below_y);
            bool agent_below = (agent_x == below_x && agent_y == below_y);

            if (obj2 == CELL_SPACE && !agent_below) {
                set_obj_i(i, CELL_SPACE);
                set_obj(below_x, below_y, moving(obj));
            } else if (agent_below && is_moving(obj)) {
                terminated = true;
            } else if (is_round(obj2) && obj_x > 0
                       && is_free(obj_x - 1, obj_y)
                       && is_free(obj_x - 1, obj_y - 1)) {
                set_obj_i(i, CELL_SPACE);
                set_obj(obj_x - 1, obj_y, stationary(obj));
            } else if (is_round(obj2) && obj_x < WORLD_W - 1
                       && is_free(obj_x + 1, obj_y)
                       && is_free(obj_x + 1, obj_y - 1)) {
                set_obj_i(i, CELL_SPACE);
                set_obj(obj_x + 1, obj_y, stat);
            } else {
                set_obj_i(i, stat);
            }
        }
    }
    diamonds_remaining = diamonds_count;

    if (!terminated && agent_x == exit_x && agent_y == exit_y && diamonds_remaining == 0) {
        reward += COMPLETION_BONUS;
        terminated = true;
    }

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
                 float cam_x, float cam_y, float pix, int w, int h, bool flip) {
    if (!tex || tex->width == 0) return;
    SDL_FRect dst;
    dst.w = 2.f * rx * pix;
    dst.h = 2.f * ry * pix;
    dst.x = (x - cam_x) * pix + 0.5f * (float)w - rx * pix;
    dst.y = (cam_y - y) * pix + 0.5f * (float)h - ry * pix;
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

    float view = (float)WORLD_H;
    float pix  = (float)height / view;
    float cam_x = 0.5f * WORLD_W;
    float cam_y = 0.5f * WORLD_H;
    float wr = 0.5f;

    if (!bg_textures.empty()) {
        Asset_Texture* bg = &bg_textures[bg_index];
        if (bg->width > 0) {
            SDL_FRect dst{ 0, 0, (float)width, (float)height };
            SDL_Texture* t = is_obs ? bg->obs_texture : bg->window_texture;
            SDL_RenderTexture(gr.get_renderer(), t, nullptr, &dst);
        }
    }

    blit(&tex_exit, exit_x + 0.5f, exit_y + 0.5f, wr, wr,
         cam_x, cam_y, pix, width, height, false);

    for (int gy = 0; gy < WORLD_H; gy++) {
        for (int gx = 0; gx < WORLD_W; gx++) {
            int t = get_obj(gx, gy);
            Asset_Texture* tex = nullptr;
            if (t == CELL_DIRT) tex = &tex_dirt;
            else if (t == CELL_BOULDER || t == CELL_MOVING_BOULDER) tex = &tex_boulder;
            else if (t == CELL_DIAMOND || t == CELL_MOVING_DIAMOND) tex = &tex_diamond;
            if (tex)
                blit(tex, gx + 0.5f, gy + 0.5f, wr, wr, cam_x, cam_y, pix, width, height, false);
        }
    }

    blit(&tex_player, agent_x + 0.5f, agent_y + 0.5f, wr, wr,
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
    tex_player = Asset_Texture();
    tex_boulder = Asset_Texture();
    tex_diamond = Asset_Texture();
    tex_exit = Asset_Texture();
    tex_dirt = Asset_Texture();
    tex_brick = Asset_Texture();
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
