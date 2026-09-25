#include "../../cenv/cenv.h"

#include <cassert>
#include <cmath>
#include <vector>
#include <algorithm>
#include <random>

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

// World dimensions (matches original: 16×16 units)
const float WORLD_W = 16.0f;
const float WORLD_H = 16.0f;

// Physics (hard mode, matching original starpilot.cpp + basic-abstract-game.cpp)
const float V_SCALE      = 2.0f / 5.0f;   // 0.4
const float MIXRATE      = 0.5f;
const float MAXSPEED     = 0.75f;
const float DECAY        = 0.9f;           // per-step velocity decay

// Object speeds in world-units/step (after V_SCALE)
const float SPD_FLYER        = 1.0f  * V_SCALE;  // 0.40
const float SPD_FAST_FLYER   = 1.5f  * V_SCALE;  // 0.60
const float SPD_SLOW         = 0.5f  * V_SCALE;  // 0.20 (meteors/clouds/turrets/bg)
const float SPD_BULLET_PLAYER= 2.0f  * V_SCALE;  // 0.80
const float SPD_BULLET_ENEMY = 2.0f  * V_SCALE;  // 0.80

const float ENEMY_REWARD     = 1.0f;
const float COMPLETION_BONUS = 10.0f;

const int SHOOTER_WIN_TIME   = 500;   // finish line spawns at this step
const int TIMEOUT            = 1000;  // episode ends (truncated) if no win

const int MIN_DELTA_T  = 10;
const int MAX_DELTA_T  = 30;
const int MAX_GROUP    = 5;
const int NUM_THEMES   = 7;           // spaceShips_001 to 007

// ---------------------- Entity Types ----------------------

enum ObjType {
    OBJ_DEAD = 0,
    OBJ_FLYER,
    OBJ_FAST_FLYER,
    OBJ_METEOR,
    OBJ_CLOUD,
    OBJ_TURRET,
    OBJ_BULLET_PLAYER,
    OBJ_BULLET_ENEMY,
    OBJ_FINISH_LINE,
    OBJ_EXPLOSION
};

struct SpaceObj {
    ObjType type     = OBJ_DEAD;
    float x = 0, y = 0;
    float vx = 0, vy = 0;
    float rx = 0;           // half-width / radius
    float ry = 0;           // half-height
    float rotation  = 0;
    float health    = 0;
    int   fire_abs  = -1;   // absolute step to fire once (-1 = periodic)
    int   fire_per  = -1;   // period for periodic firing (turrets), -1 = once
    int   theme     = 0;
    float alpha     = 1.0f;
    int   exp_frame = 0;    // explosion animation (0–4)
    bool  will_erase = false;
};

struct Spawner {
    int spawn_time;
    SpaceObj obj;
};

// ---------------------- Textures ----------------------

struct TexGroup {
    std::vector<Asset_Texture> textures;
    Asset_Texture& operator[](int i) { return textures[i % textures.size()]; }
    size_t size() const { return textures.size(); }
    void load(const std::string& base, int n) {
        textures.resize(n);
        for (int i = 0; i < n; i++) {
            char buf[256]; snprintf(buf, sizeof(buf), (base + "_%03d.png").c_str(), i + 1);
            textures[i].load(buf);
        }
    }
    void loadv(std::initializer_list<std::string> names) {
        textures.resize(names.size());
        int i = 0;
        for (auto& n : names) textures[i++].load(n);
    }
};

Asset_Texture tex_player;
TexGroup      tex_flyers;      // spaceShips_001-007 (7)
TexGroup      tex_meteors;     // 4 spaceMeteors + 4 meteorGrey = 8
TexGroup      tex_clouds;      // spaceEffect1-9 (9)
TexGroup      tex_turrets;     // spaceStation_018, 019 (2)
TexGroup      tex_bullet_p;    // towerDefense_tile295
TexGroup      tex_bullet_e;    // towerDefense_tile296
TexGroup      tex_bullet_t;    // towerDefense_tile297 (turret bullet)
TexGroup      tex_finish;      // spaceRockets_001-004
TexGroup      tex_explosion;   // explosion1-5
std::vector<Asset_Texture> bg_textures;

// ---------------------- Game State ----------------------

std::mt19937 rng;

SDL_Surface* window_target = nullptr;
SDL_Surface* obs_target    = nullptr;
SDL_Renderer* window_renderer = nullptr;
SDL_Renderer* obs_renderer    = nullptr;

uint32_t rmask, gmask, bmask, amask;

// Player
float player_x, player_y;
float player_vx, player_vy;
float player_rx = 0.4f, player_ry = 0.4f;
bool  player_alive;
int   player_theme;

// All active game entities
std::vector<SpaceObj> entities;

// Pre-computed spawner list (sorted descending by spawn_time for pop_back)
std::vector<Spawner> spawners;

int cur_time;
int bg_index;

// ---------------------- Forward declarations ----------------------

void render_game(bool is_obs);
void reset_game();

// ---------------------- Helpers ----------------------

static float rand01() {
    return std::uniform_real_distribution<float>(0.f, 1.f)(rng);
}
static float rand_range(float lo, float hi) {
    return std::uniform_real_distribution<float>(lo, hi)(rng);
}
static int rand_int(int lo, int hi_inclusive) {
    return std::uniform_int_distribution<int>(lo, hi_inclusive)(rng);
}

// Circle overlap test
static bool circles_overlap(float x1, float y1, float r1,
                             float x2, float y2, float r2) {
    float dx = x1 - x2, dy = y1 - y2;
    float sum = r1 + r2;
    return dx*dx + dy*dy < sum*sum;
}

// ---------------------- Spawner generation ----------------------

static void gen_spawners() {
    spawners.clear();

    int t = 1 + rand_int(MIN_DELTA_T, MAX_DELTA_T);

    // Object radii (in world units, from original hp_object_r)
    auto obj_r = [](ObjType tp) -> float {
        if (tp == OBJ_TURRET || tp == OBJ_METEOR || tp == OBJ_CLOUD) return 1.0f;
        return 0.5f;  // flyer / fast_flyer
    };

    // Spawn-weight table (hard mode): FLYER×3, FAST_FLYER×1, METEOR×1, CLOUD×1, TURRET×1
    struct WEntry { ObjType type; int weight; };
    const WEntry table[] = {
        { OBJ_FLYER,      3 },
        { OBJ_FAST_FLYER, 1 },
        { OBJ_METEOR,     1 },
        { OBJ_CLOUD,      1 },
        { OBJ_TURRET,     1 },
    };
    const int total_w = 8;

    while (t <= SHOOTER_WIN_TIME) {
        // Weighted random pick
        int pick = rand_int(0, total_w - 1);
        ObjType type = OBJ_FLYER;
        int acc = 0;
        for (auto& e : table) {
            acc += e.weight;
            if (pick < acc) { type = e.type; break; }
        }

        float r = obj_r(type);
        int group_size = 1;
        int theme = 0;

        if (type == OBJ_FLYER || type == OBJ_FAST_FLYER) {
            group_size = rand_int(1, MAX_GROUP);
            theme = rand_int(0, NUM_THEMES - 1);
        }

        for (int j = 0; j < group_size; j++) {
            int spawn_time = t + j * 5;

            float y_pos = rand_range(r, WORLD_H - r);

            SpaceObj obj;
            obj.type  = type;
            obj.rx    = r;
            obj.ry    = r;
            obj.theme = theme;

            if (type == OBJ_METEOR || type == OBJ_CLOUD) {
                // Slow, straight left, no firing
                obj.vx      = -SPD_SLOW;
                obj.vy      = 0;
                obj.fire_abs = -1;
                obj.health  = (type == OBJ_METEOR) ? 500.f : 0.f;
                obj.rotation = 0;
                theme = rand_int(0, (type == OBJ_METEOR ? 7 : 8));
                obj.theme = theme;
            } else if (type == OBJ_TURRET) {
                // Slow left, fires periodically
                obj.vx       = -SPD_SLOW;
                obj.vy       = 0;
                obj.fire_per = rand_int(20, 30);
                obj.health   = 5.f;
                obj.theme    = rand_int(0, 1);
            } else {
                // Flyer / fast flyer: angled trajectory from right
                float k = 2.f * (float)M_PI / 4.f;
                float theta = (rand01() - .5f) * k;
                if (rand_int(0, 1) == 1) theta = 0;

                float speed = (type == OBJ_FAST_FLYER) ? SPD_FAST_FLYER : SPD_FLYER;
                obj.vx = -cosf(theta) * speed;
                obj.vy =  sinf(theta) * speed;
                obj.fire_abs = spawn_time + rand_int(10, 100);
                obj.health   = (type == OBJ_FAST_FLYER) ? 1.f : 2.f;
                obj.rotation = (float)M_PI / 2.f;  // rotated so nose points left
            }

            obj.x = WORLD_W + r;
            obj.y = y_pos;

            Spawner sp;
            sp.spawn_time = spawn_time;
            sp.obj = obj;
            spawners.push_back(sp);
        }

        t += rand_int(MIN_DELTA_T, MAX_DELTA_T);
    }

    // Sort descending so we can pop_back cheaply
    std::sort(spawners.begin(), spawners.end(),
              [](const Spawner& a, const Spawner& b){ return a.spawn_time > b.spawn_time; });
}

// ---------------------- CEnv Interface Implementation ----------------------

int32_t cenv_get_env_version() { return version; }

int32_t cenv_make(const char* render_mode, cenv_option* options, int32_t options_size) {
    if (g_initialized) cenv_close();
    g_initialized = true;

    unsigned int seed = (unsigned int)time(nullptr);

    for (int i = 0; i < options_size; i++) {
        std::string name(options[i].name);
        if (name == "seed")   { seed = (unsigned int)options[i].value.i; }
        if (name == "width")  { window_width  = options[i].value.i; }
        if (name == "height") { window_height = options[i].value.i; }
    }

    // ---- CEnv buffers ----
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

    // ---- SDL ----
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
        sdl_window    = SDL_CreateWindow("StarPilot", window_width, window_height, 0);
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

    // ---- Load assets ----
    rng.seed(seed);

    tex_player.load("assets/misc_assets/playerShip2_blue.png");

    tex_flyers.loadv({
        "assets/misc_assets/spaceShips_001.png",
        "assets/misc_assets/spaceShips_002.png",
        "assets/misc_assets/spaceShips_003.png",
        "assets/misc_assets/spaceShips_004.png",
        "assets/misc_assets/spaceShips_005.png",
        "assets/misc_assets/spaceShips_006.png",
        "assets/misc_assets/spaceShips_007.png",
    });

    tex_meteors.loadv({
        "assets/misc_assets/spaceMeteors_001.png",
        "assets/misc_assets/spaceMeteors_002.png",
        "assets/misc_assets/spaceMeteors_003.png",
        "assets/misc_assets/spaceMeteors_004.png",
        "assets/misc_assets/meteorGrey_big1.png",
        "assets/misc_assets/meteorGrey_big2.png",
        "assets/misc_assets/meteorGrey_big3.png",
        "assets/misc_assets/meteorGrey_big4.png",
    });

    tex_clouds.loadv({
        "assets/misc_assets/spaceEffect1.png",
        "assets/misc_assets/spaceEffect2.png",
        "assets/misc_assets/spaceEffect3.png",
        "assets/misc_assets/spaceEffect4.png",
        "assets/misc_assets/spaceEffect5.png",
        "assets/misc_assets/spaceEffect6.png",
        "assets/misc_assets/spaceEffect7.png",
        "assets/misc_assets/spaceEffect8.png",
        "assets/misc_assets/spaceEffect9.png",
    });

    tex_turrets.loadv({
        "assets/misc_assets/spaceStation_018.png",
        "assets/misc_assets/spaceStation_019.png",
    });

    tex_bullet_p.loadv({ "assets/misc_assets/towerDefense_tile295.png" });
    tex_bullet_e.loadv({ "assets/misc_assets/towerDefense_tile296.png" });
    tex_bullet_t.loadv({ "assets/misc_assets/towerDefense_tile297.png" });

    tex_finish.loadv({
        "assets/misc_assets/spaceRockets_001.png",
        "assets/misc_assets/spaceRockets_002.png",
        "assets/misc_assets/spaceRockets_003.png",
        "assets/misc_assets/spaceRockets_004.png",
    });

    tex_explosion.loadv({
        "assets/misc_assets/explosion1.png",
        "assets/misc_assets/explosion2.png",
        "assets/misc_assets/explosion3.png",
        "assets/misc_assets/explosion4.png",
        "assets/misc_assets/explosion5.png",
    });

    // Space backgrounds (same list as CaveFlyer/BossFight)
    std::vector<std::string> bg_names = {
        "assets/space_backgrounds/deep_space_01.png",
        "assets/space_backgrounds/spacegen_01.png",
        "assets/space_backgrounds/milky_way_01.png",
        "assets/space_backgrounds/ez_space_lite_01.png",
        "assets/space_backgrounds/meyespace_v1_01.png",
        "assets/space_backgrounds/eye_nebula_01.png",
        "assets/space_backgrounds/deep_sky_01.png",
        "assets/space_backgrounds/space_nebula_01.png",
        "assets/space_backgrounds/Background-1.png",
        "assets/space_backgrounds/Background-2.png",
        "assets/space_backgrounds/Background-3.png",
        "assets/space_backgrounds/Background-4.png",
        "assets/space_backgrounds/parallax-space-backgound.png",
    };
    bg_textures.resize(bg_names.size());
    for (int i = 0; i < (int)bg_names.size(); i++)
        bg_textures[i].load(bg_names[i]);

    reset_game();
    return 0;
}

// ---------------------- Game reset ----------------------

void reset_game() {
    entities.clear();
    cur_time = 0;

    bg_index = rand_int(0, (int)bg_textures.size() - 1);
    player_theme = rand_int(0, 3);  // 4 color variants of playerShip2

    // Player starts near left-centre
    player_x  = 2.0f;
    player_y  = rand_range(2.f, WORLD_H - 2.f);
    player_vx = 0;
    player_vy = 0;
    player_alive = true;

    gen_spawners();
}

// ---------------------- Game step helpers ----------------------

static void render_entity(const SpaceObj& e, Asset_Texture* tex) {
    if (!tex || tex->width == 0) return;
    float scale = (e.rx * 2.f * unit_to_pixels) / tex->width;
    float sx = (e.x - e.rx) * unit_to_pixels;
    float sy = (e.y - e.ry) * unit_to_pixels;
    if (e.rotation != 0.f) {
        Vector2 centre{ (e.x) * unit_to_pixels, (e.y) * unit_to_pixels };
        gr.render_texture_rotated(tex, centre, e.rotation, scale, e.alpha);
    } else {
        gr.render_texture(tex, { sx, sy }, scale, e.alpha);
    }
}

// ---------------------- Step ----------------------

int32_t cenv_step(cenv_key_value* actions, int32_t actions_size) {
    int action = 4;  // default: stand still

    for (int i = 0; i < actions_size; i++) {
        std::string key(actions[i].key);
        if (key == "action") {
            assert(actions[i].value_type == CENV_VALUE_TYPE_INT);
            assert(actions[i].value_buffer_size == 1);
            action = actions[i].value_buffer.i[0];
        }
    }

    if (!player_alive) {
        step_data.reward.f = 0.f;
        step_data.terminated = true;
        step_data.truncated  = false;
        render_game(true);
        // copy pixels
        SDL_LockSurface(obs_target);
        uint8_t* px = (uint8_t*)obs_target->pixels;
        for (int x = 0; x < obs_width; x++)
            for (int y = 0; y < obs_height; y++) {
                observation.value_buffer.b[0+3*(y+obs_height*x)] = px[0+4*(y+obs_height*x)];
                observation.value_buffer.b[1+3*(y+obs_height*x)] = px[1+4*(y+obs_height*x)];
                observation.value_buffer.b[2+3*(y+obs_height*x)] = px[2+4*(y+obs_height*x)];
            }
        SDL_UnlockSurface(obs_target);
        return 0;
    }

    cur_time++;

    // ---- Action mapping (matches original basic-abstract-game.cpp) ----
    int move_action    = action % 9;
    int special_action = 0;
    if (action >= 9) {
        special_action = action - 8;   // 1 = fire right, 2 = fire left
        move_action    = 4;            // stand still when firing
    }

    float avx = (float)(move_action / 3 - 1);  // -1, 0, 1
    float avy = (float)(move_action % 3 - 1);  // -1, 0, 1

    // update_agent_velocity + decay (matches original)
    player_vx = DECAY * ((1.f - MIXRATE) * player_vx + MIXRATE * MAXSPEED * avx);
    player_vy = DECAY * ((1.f - MIXRATE) * player_vy + MIXRATE * MAXSPEED * avy);

    player_x += player_vx;
    player_y += player_vy;

    // Clamp player to world bounds
    player_x = std::max(player_rx, std::min(WORLD_W - player_rx, player_x));
    player_y = std::max(player_ry, std::min(WORLD_H - player_ry, player_y));

    // ---- Spawn pre-computed entities ----
    while (!spawners.empty() && spawners.back().spawn_time <= cur_time) {
        entities.push_back(spawners.back().obj);
        spawners.pop_back();
    }

    // ---- Fire: spawn finish line at SHOOTER_WIN_TIME ----
    if (cur_time == SHOOTER_WIN_TIME) {
        SpaceObj fl;
        fl.type  = OBJ_FINISH_LINE;
        fl.x     = WORLD_W + 2.f;
        fl.y     = WORLD_H * 0.5f;
        fl.vx    = -SPD_SLOW;
        fl.vy    = 0;
        fl.rx    = 2.0f;
        fl.ry    = 2.0f;
        fl.theme = rand_int(0, 3);
        entities.push_back(fl);
    }

    // ---- Player fires ----
    if (special_action == 1 || special_action == 2) {
        float theta = (special_action == 2) ? (float)M_PI : 0.f;
        SpaceObj b;
        b.type  = OBJ_BULLET_PLAYER;
        b.x     = player_x + (special_action == 2 ? -player_rx : player_rx);
        b.y     = player_y;
        b.vx    = cosf(theta) * SPD_BULLET_PLAYER;
        b.vy    = sinf(theta) * SPD_BULLET_PLAYER;
        b.rx    = 0.15f;
        b.ry    = 0.15f;
        b.rotation = theta - (float)M_PI / 2.f;
        entities.push_back(b);
    }

    float reward = 0.f;
    bool  terminated = false;

    // ---- Update all entities ----
    for (auto& e : entities) {
        if (e.will_erase) continue;

        // Move
        e.x += e.vx;
        e.y += e.vy;

        // Turret fires periodically
        if (e.type == OBJ_TURRET && e.fire_per > 0 &&
            (cur_time % e.fire_per == 0)) {
            float dx = player_x - e.x;
            float dy = player_y - e.y;
            float d  = sqrtf(dx*dx + dy*dy);
            if (d > 0.001f) {
                SpaceObj b;
                b.type = OBJ_BULLET_ENEMY;
                b.x  = e.x; b.y = e.y;
                b.vx = (dx / d) * SPD_BULLET_ENEMY;
                b.vy = (dy / d) * SPD_BULLET_ENEMY;
                b.rx = 0.15f; b.ry = 0.15f;
                float ang = atan2f(dy, dx);
                b.rotation = ang - (float)M_PI / 2.f;
                entities.push_back(b);
            }
        }

        // Flyer/fast-flyer fires once at fire_abs step
        if ((e.type == OBJ_FLYER || e.type == OBJ_FAST_FLYER) &&
            e.fire_abs == cur_time) {
            float dx = player_x - e.x;
            float dy = player_y - e.y;
            float d  = sqrtf(dx*dx + dy*dy);
            if (d > 0.001f) {
                SpaceObj b;
                b.type = OBJ_BULLET_ENEMY;
                b.x  = e.x; b.y = e.y;
                b.vx = (dx / d) * SPD_BULLET_ENEMY;
                b.vy = (dy / d) * SPD_BULLET_ENEMY;
                b.rx = 0.15f; b.ry = 0.15f;
                float ang = atan2f(dy, dx);
                b.rotation = ang - (float)M_PI / 2.f;
                entities.push_back(b);
            }
        }

        // Expire explosions
        if (e.type == OBJ_EXPLOSION) {
            e.exp_frame++;
            e.alpha = 1.f - (float)e.exp_frame / 5.f;
            if (e.exp_frame >= 5) e.will_erase = true;
        }

        // Erase entities that have scrolled off screen
        if (e.type != OBJ_EXPLOSION) {
            if (e.x + e.rx < -2.f || e.x - e.rx > WORLD_W + 2.f ||
                e.y + e.ry < -2.f || e.y - e.ry > WORLD_H + 2.f)
                e.will_erase = true;
        }
    }

    // ---- Collision detection (second pass, after positions updated) ----
    for (auto& e : entities) {
        if (e.will_erase) continue;

        // Player bullet hits enemy
        if (e.type == OBJ_BULLET_PLAYER) {
            for (auto& t : entities) {
                if (t.will_erase) continue;
                if (t.type != OBJ_FLYER && t.type != OBJ_FAST_FLYER &&
                    t.type != OBJ_TURRET && t.type != OBJ_METEOR) continue;

                if (circles_overlap(e.x, e.y, e.rx, t.x, t.y, t.rx)) {
                    e.will_erase = true;
                    t.health -= 1.f;

                    // Spawn explosion at hit point
                    SpaceObj exp;
                    exp.type = OBJ_EXPLOSION;
                    exp.x = t.x; exp.y = t.y;
                    exp.rx = t.rx * 0.5f; exp.ry = t.ry * 0.5f;
                    entities.push_back(exp);

                    if (t.health <= 0.f &&
                        (t.type == OBJ_FLYER || t.type == OBJ_FAST_FLYER ||
                         t.type == OBJ_TURRET)) {
                        t.will_erase = true;
                        reward += ENEMY_REWARD;
                    }
                    break;
                }
            }
        }

        // Enemy / enemy bullet hits player
        bool lethal = (e.type == OBJ_FLYER || e.type == OBJ_FAST_FLYER ||
                       e.type == OBJ_BULLET_ENEMY || e.type == OBJ_TURRET ||
                       e.type == OBJ_METEOR);
        if (lethal && circles_overlap(e.x, e.y, e.rx,
                                       player_x, player_y,
                                       (player_rx + player_ry) * 0.5f)) {
            player_alive = false;
            terminated   = true;
        }

        // Player reaches finish line
        if (e.type == OBJ_FINISH_LINE &&
            circles_overlap(e.x, e.y, e.rx,
                            player_x, player_y,
                            (player_rx + player_ry) * 0.5f)) {
            reward     += COMPLETION_BONUS;
            terminated  = true;
        }
    }

    // ---- Erase dead entities ----
    entities.erase(std::remove_if(entities.begin(), entities.end(),
        [](const SpaceObj& e){ return e.will_erase; }), entities.end());

    // ---- Truncation ----
    bool truncated = (!terminated && cur_time >= TIMEOUT);

    step_data.reward.f   = reward;
    step_data.terminated = terminated;
    step_data.truncated  = truncated;

    // ---- Render obs ----
    render_game(true);

    SDL_LockSurface(obs_target);
    uint8_t* px = (uint8_t*)obs_target->pixels;
    for (int x = 0; x < obs_width; x++)
        for (int y = 0; y < obs_height; y++) {
            observation.value_buffer.b[0+3*(y+obs_height*x)] = px[0+4*(y+obs_height*x)];
            observation.value_buffer.b[1+3*(y+obs_height*x)] = px[1+4*(y+obs_height*x)];
            observation.value_buffer.b[2+3*(y+obs_height*x)] = px[2+4*(y+obs_height*x)];
        }
    SDL_UnlockSurface(obs_target);

    return 0;
}

// ---------------------- Reset ----------------------

int32_t cenv_reset(cenv_option* options, int32_t options_size) {
    for (int i = 0; i < options_size; i++) {
        std::string name(options[i].name);
        if (name == "seed") { rng.seed((unsigned int)options[i].value.i); }
    }

    reset_game();
    render_game(true);

    SDL_LockSurface(obs_target);
    uint8_t* px = (uint8_t*)obs_target->pixels;
    for (int x = 0; x < obs_width; x++)
        for (int y = 0; y < obs_height; y++) {
            observation.value_buffer.b[0+3*(y+obs_height*x)] = px[0+4*(y+obs_height*x)];
            observation.value_buffer.b[1+3*(y+obs_height*x)] = px[1+4*(y+obs_height*x)];
            observation.value_buffer.b[2+3*(y+obs_height*x)] = px[2+4*(y+obs_height*x)];
        }
    SDL_UnlockSurface(obs_target);

    return 0;
}

// ---------------------- Rendering ----------------------

void render_game(bool is_obs) {
    gr.rendering_obs = is_obs;
    int width  = is_obs ? obs_width  : window_width;
    int height = is_obs ? obs_height : window_height;

    SDL_SetRenderDrawColor(gr.get_renderer(), 0, 0, 0, 255);
    SDL_RenderClear(gr.get_renderer());

    // Scale: world units → pixels. Positions passed to render_texture are in
    // world units; the renderer then does (pos - camera) * scale + screen/2.
    // Centre the camera on the 16×16 playfield so world (0,0) maps to the
    // top-left of the surface.
    float scale = (float)height / WORLD_H;

    gr.camera_scale = scale;
    gr.camera_size  = { (float)width, (float)height };
    gr.camera_position = { WORLD_W * 0.5f, WORLD_H * 0.5f };

    // ---- Scrolling background ----
    if (!bg_textures.empty()) {
        Asset_Texture* bg = &bg_textures[bg_index];
        if (bg->width > 0) {
            // Background scrolls left at SPD_SLOW; wrap around every bg-width
            float bg_scale = (float)height / bg->height;
            float bg_world_w = bg->width * bg_scale;
            float offset_px = fmodf((float)cur_time * SPD_SLOW * scale, bg_world_w);

            // Draw two tiles side-by-side so the seam is never visible
            for (int tile = -1; tile <= 1; tile++) {
                float bx = tile * bg_world_w - offset_px;
                SDL_FRect dst{ bx, 0, bg_world_w, (float)height };
                SDL_RenderTexture(gr.get_renderer(),
                    is_obs ? bg->obs_texture : bg->window_texture,
                    nullptr, &dst);
            }
        }
    }

    // ---- Draw entities ----
    // Clouds first (background layer), then everything else
    for (int pass = 0; pass < 2; pass++) {
        for (const auto& e : entities) {
            if (e.will_erase) continue;

            bool is_cloud = (e.type == OBJ_CLOUD);
            if (pass == 0 && !is_cloud) continue;
            if (pass == 1 &&  is_cloud) continue;

            Asset_Texture* tex = nullptr;
            float alpha = e.alpha;

            switch (e.type) {
                case OBJ_FLYER:
                case OBJ_FAST_FLYER: tex = &tex_flyers[e.theme]; break;
                case OBJ_METEOR:     tex = &tex_meteors[e.theme]; break;
                case OBJ_CLOUD:      tex = &tex_clouds[e.theme]; alpha = 0.6f; break;
                case OBJ_TURRET:     tex = &tex_turrets[e.theme]; break;
                case OBJ_BULLET_PLAYER: tex = &tex_bullet_p[0]; break;
                case OBJ_BULLET_ENEMY:  tex = &tex_bullet_e[0]; break;
                case OBJ_FINISH_LINE:   tex = &tex_finish[e.theme]; break;
                case OBJ_EXPLOSION:
                    tex = &tex_explosion[std::min(e.exp_frame, 4)]; break;
                default: break;
            }

            if (!tex || tex->width == 0) continue;

            // World-space size / texture pixels — camera_scale converts to pixels.
            float obj_scale = (e.rx * 2.f) / tex->width;
            Vector2 top_left{ e.x - e.rx, e.y - e.ry };

            if (e.rotation != 0.f) {
                gr.render_texture_rotated(tex, top_left, e.rotation, obj_scale, alpha);
            } else {
                gr.render_texture(tex, top_left, obj_scale, alpha);
            }
        }
    }

    // ---- Draw player ----
    if (player_alive && tex_player.width > 0) {
        float obj_scale = (player_rx * 2.f) / tex_player.width;
        Vector2 top_left{ player_x - player_rx, player_y - player_ry };
        gr.render_texture(&tex_player, top_left, obj_scale, 1.0f,
                          /*flip_x=*/player_vx < 0);
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

    SDL_LockSurface(window_target);
    uint8_t* px = (uint8_t*)window_target->pixels;
    for (int x = 0; x < window_width; x++)
        for (int y = 0; y < window_height; y++) {
            render_data.value_buffer.b[0+3*(y+window_height*x)] = px[0+4*(y+window_height*x)];
            render_data.value_buffer.b[1+3*(y+window_height*x)] = px[1+4*(y+window_height*x)];
            render_data.value_buffer.b[2+3*(y+window_height*x)] = px[2+4*(y+window_height*x)];
        }
    SDL_UnlockSurface(window_target);
    return 0;
}

// ---------------------- Close ----------------------

void cenv_close() {
    if (!g_initialized) return;

    entities.clear();
    spawners.clear();

    // Destroy textures before renderers
    tex_player = Asset_Texture();
    tex_flyers.textures.clear();
    tex_meteors.textures.clear();
    tex_clouds.textures.clear();
    tex_turrets.textures.clear();
    tex_bullet_p.textures.clear();
    tex_bullet_e.textures.clear();
    tex_bullet_t.textures.clear();
    tex_finish.textures.clear();
    tex_explosion.textures.clear();
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
    free(observation.value_buffer.b);  observation.value_buffer.b = nullptr;
    free(render_data.value_buffer.b);  render_data.value_buffer.b = nullptr;

    g_human_mode  = false;
    g_initialized = false;
}
