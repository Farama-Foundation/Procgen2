#include "../../cenv/cenv.h"

#include <cmath>
#include <iostream>
#include <random>

#include "common_systems.h"

const int version = 100;
const bool show_log = false;

// ---------------------- CEnv Interface ----------------------

// Define globals from cenv
cenv_make_data make_data;
cenv_reset_data reset_data;
cenv_step_data step_data;
cenv_render_data render_data;

// Shared value between different datas (optional)
cenv_key_value observation;

// ---------------------- Game ----------------------

const int obs_width = 64;
const int obs_height = 64;
const int num_actions = 15;

int window_width = 512;
int window_height = 512;

float game_zoom = 1.0f; // Base game zoom level

std::mt19937 rng;

SDL_Surface* window_target; // Main render window
SDL_Surface* obs_target; // Observation target
SDL_Renderer* window_renderer;
SDL_Renderer* obs_renderer;

// Masking for SDL surfaces
uint32_t rmask, gmask, bmask, amask;

const int sub_steps = 4; // Physics sub-steps
float dt = 1.0f / sub_steps; // Not relative to time in seconds, need to do it this way due to the weird way the original procgen works w.r.t. physics

// Systems
std::shared_ptr<System_Sprite_Render> sprite_render;
std::shared_ptr<System_Hazard> hazard;
std::shared_ptr<System_Mob_AI> mob_ai;
std::shared_ptr<System_Agent> agent;

// Big list of different background images
std::vector<std::string> background_names {
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
    "assets/space_backgrounds/parallax-space-backgound.png"
};

std::vector<std::string> barrier_names = {
    "assets/misc_assets/spaceMeteors_001.png",
    "assets/misc_assets/spaceMeteors_002.png",
    "assets/misc_assets/spaceMeteors_003.png",
    "assets/misc_assets/spaceMeteors_004.png",
    "assets/misc_assets/meteorGrey_big1.png",
    "assets/misc_assets/meteorGrey_big2.png",
    "assets/misc_assets/meteorGrey_big3.png",
    "assets/misc_assets/meteorGrey_big4.png"
};

std::vector<Asset_Texture> background_textures;
std::vector<Asset_Texture> barrier_textures;

int current_background_index = 0;
float current_background_offset_x = 0.0f;
float current_background_offset_y = 0.0f;

// Forward declarations
void render_game(bool is_obs);
void reset();

int32_t cenv_get_env_version() {
    return version;
}

int32_t cenv_make(const char* render_mode, cenv_option* options, int32_t options_size) {
    // ---------------------- CEnv Interface ----------------------

    unsigned int seed = time(nullptr);

    // Parse options
    for (int i = 0; i < options_size; i++) {
        std::string name(options[i].name);

        if (name == "seed") {
            assert(options[i].value_type == CENV_VALUE_TYPE_INT);

            seed = options[i].value.i;
        }
        else if (name == "width") {
            assert(options[i].value_type == CENV_VALUE_TYPE_INT);

            window_width = options[i].value.i;
        }
        else if (name == "height") {
            assert(options[i].value_type == CENV_VALUE_TYPE_INT);

            window_height = options[i].value.i;
        }
    }
    
    // Allocate make data
    make_data.observation_spaces_size = 1;
    make_data.observation_spaces = (cenv_key_value*)malloc(sizeof(cenv_key_value));

    make_data.observation_spaces[0].key = "screen";
    make_data.observation_spaces[0].value_type = CENV_SPACE_TYPE_BOX;
    make_data.observation_spaces[0].value_buffer_size = 2; // Low and high

    make_data.observation_spaces[0].value_buffer.f = (float*)malloc(make_data.observation_spaces[0].value_buffer_size * sizeof(float));

    // Low and high
    make_data.observation_spaces[0].value_buffer.f[0] = 0.0f;
    make_data.observation_spaces[0].value_buffer.f[1] = 255.0f;

    make_data.action_spaces_size = 1;
    make_data.action_spaces = (cenv_key_value*)malloc(sizeof(cenv_key_value));

    make_data.action_spaces[0].key = "action";
    make_data.action_spaces[0].value_type = CENV_SPACE_TYPE_MULTI_DISCRETE;
    make_data.action_spaces[0].value_buffer_size = 1;

    make_data.action_spaces[0].value_buffer.i = (int32_t*)malloc(sizeof(int32_t));
    make_data.action_spaces[0].value_buffer.i[0] = num_actions;

    // Allocate observations once and re-use (doesn't resize dynamically)
    observation.key = "screen";
    observation.value_type = CENV_VALUE_TYPE_BYTE;
    observation.value_buffer_size = obs_width * obs_height * 3;
    observation.value_buffer.b = (uint8_t*)malloc(obs_width * obs_height * 3 * sizeof(uint8_t));

    // Reset data
    reset_data.observations_size = 1;
    reset_data.observations = &observation;
    reset_data.infos_size = 0;
    reset_data.infos = NULL;

    // Step data
    step_data.observations_size = 1;
    step_data.observations = &observation;
    step_data.reward.f = 0.0f;
    step_data.terminated = false;
    step_data.truncated = false;
    step_data.infos_size = 0;
    step_data.infos = NULL;

    // Frame
    render_data.value_type = CENV_VALUE_TYPE_BYTE;
    render_data.value_buffer_height = window_height;
    render_data.value_buffer_width = window_width;
    render_data.value_buffer_channels = 3;
    render_data.value_buffer.b = (uint8_t*)malloc(window_width * window_height * 3 * sizeof(uint8_t));

    // ---------------------- Game ----------------------

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    SDL_Init(SDL_INIT_VIDEO);

    IMG_Init(IMG_INIT_PNG);

    window_target = SDL_CreateSurface(window_width, window_height, SDL_GetPixelFormatEnumForMasks(32, rmask, gmask, bmask, amask));
    obs_target = SDL_CreateSurface(obs_width, obs_height, SDL_GetPixelFormatEnumForMasks(32, rmask, gmask, bmask, amask));

    window_renderer = SDL_CreateSoftwareRenderer(window_target);
    obs_renderer = SDL_CreateSoftwareRenderer(obs_target);

    gr.window_renderer = window_renderer;
    gr.obs_renderer = obs_renderer;

    // Seed RNG
    rng.seed(seed);

    // Register components
    c.register_component<Component_Transform>();
    c.register_component<Component_Collision>();
    c.register_component<Component_Dynamics>();
    c.register_component<Component_Sprite>();
    c.register_component<Component_Mob_AI>();
    c.register_component<Component_Hazard>();
    c.register_component<Component_Agent>(); // Player

    // Sprite rendering system
    sprite_render = c.register_system<System_Sprite_Render>();
    Signature sprite_render_signature;
    sprite_render_signature.set(c.get_component_type<Component_Sprite>()); // Operate only on sprites
    c.set_system_signature<System_Sprite_Render>(sprite_render_signature);

    // Hazard system setup
    hazard = c.register_system<System_Hazard>();
    Signature hazard_signature;
    hazard_signature.set(c.get_component_type<Component_Hazard>()); // Operate only on hazards
    c.set_system_signature<System_Hazard>(hazard_signature);

    // Mob AI system setup
    mob_ai = c.register_system<System_Mob_AI>();
    Signature mob_ai_signature;
    mob_ai_signature.set(c.get_component_type<Component_Mob_AI>()); // Operate only on mob ai
    c.set_system_signature<System_Mob_AI>(mob_ai_signature);

    mob_ai->init();

    // Agent system setup
    agent = c.register_system<System_Agent>();
    Signature agent_signature;
    agent_signature.set(c.get_component_type<Component_Agent>()); // Operate only on mobs
    c.set_system_signature<System_Agent>(agent_signature);

    agent->init();

    // Load backgrounds
    background_textures.resize(background_names.size());

    for (int i = 0; i < background_names.size(); i++)
        background_textures[i].load(background_names[i]);

    // Load barriers
    barrier_textures.resize(barrier_names.size());

    for (int i = 0; i < barrier_names.size(); i++)
        barrier_textures[i].load(barrier_names[i]);

    // Reset spawns entities while generating map
    reset();

    return 0; // No error
}

int32_t cenv_reset(cenv_option* options, int32_t options_size) {
    // Parse options
    for (int i = 0; i < options_size; i++) {
        std::string name(options[i].name);

        if (name == "seed") {
            assert(options[i].value_type == CENV_VALUE_TYPE_INT);

            rng.seed(options[i].value.i);
        }
    }

    reset();

    render_game(true);

    // Grab observation
    SDL_LockSurface(obs_target);

    uint8_t* pixels = (uint8_t*)obs_target->pixels;

    for (int x = 0; x < obs_width; x++)
        for (int y = 0; y < obs_height; y++) {
            observation.value_buffer.b[0 + 3 * (y + obs_height * x)] = pixels[0 + 4 * (y + obs_height * x)];
            observation.value_buffer.b[1 + 3 * (y + obs_height * x)] = pixels[1 + 4 * (y + obs_height * x)];
            observation.value_buffer.b[2 + 3 * (y + obs_height * x)] = pixels[2 + 4 * (y + obs_height * x)];
        }

    SDL_UnlockSurface(obs_target);

    return 0; // No error
}

int32_t cenv_step(cenv_key_value* actions, int32_t actions_size) {
    int action = 0;

    // Parse actions
    for (int i = 0; i < actions_size; i++) {
        std::string key(actions[i].key);

        if (key == "action") {
            assert(actions[i].value_type == CENV_VALUE_TYPE_INT);
            assert(actions[i].value_buffer_size == 1);

            action = actions[i].value_buffer.i[0];
        }
    }

    // Sub-steps
    for (int ss = 0; ss < sub_steps; ss++) {
        // Update systems
        bool agent_alive = agent->update(dt, hazard, action, rng);

        bool boss_alive = mob_ai->update(dt, hazard, rng);

        sprite_render->update(dt);

        step_data.reward.f = (!agent_alive) * -10.0f + (!boss_alive) * 10.0f;

        step_data.terminated = !agent_alive || !boss_alive;
        step_data.truncated = false;

        if (step_data.terminated)
            break;
    }

    // Render and grab pixels
    render_game(true);

    // Grab observation
    SDL_LockSurface(obs_target);

    uint8_t* pixels = (uint8_t*)obs_target->pixels;

    for (int x = 0; x < obs_width; x++)
        for (int y = 0; y < obs_height; y++) {
            observation.value_buffer.b[0 + 3 * (y + obs_height * x)] = pixels[0 + 4 * (y + obs_height * x)];
            observation.value_buffer.b[1 + 3 * (y + obs_height * x)] = pixels[1 + 4 * (y + obs_height * x)];
            observation.value_buffer.b[2 + 3 * (y + obs_height * x)] = pixels[2 + 4 * (y + obs_height * x)];
        }

    SDL_UnlockSurface(obs_target);

    return 0; // No error
}

int32_t cenv_render() {
    render_game(false);

    // Grab pixels
    SDL_LockSurface(window_target);

    uint8_t* pixels = (uint8_t*)window_target->pixels;

    for (int x = 0; x < window_width; x++)
        for (int y = 0; y < window_height; y++) {
            render_data.value_buffer.b[0 + 3 * (y + window_height * x)] = pixels[0 + 4 * (y + window_height * x)];
            render_data.value_buffer.b[1 + 3 * (y + window_height * x)] = pixels[1 + 4 * (y + window_height * x)];
            render_data.value_buffer.b[2 + 3 * (y + window_height * x)] = pixels[2 + 4 * (y + window_height * x)];
        }

    SDL_UnlockSurface(window_target);

    return 0; // No error
}

void cenv_close() {
    // ---------------------- CEnv Interface ----------------------
    
    // Dealloc make data
    for (int i = 0; i < make_data.observation_spaces_size; i++)
        free(make_data.observation_spaces[i].value_buffer.f);

    free(make_data.observation_spaces);

    for (int i = 0; i < make_data.action_spaces_size; i++)
        free(make_data.action_spaces[i].value_buffer.i);

    free(make_data.action_spaces);

    // Observations
    free(observation.value_buffer.b);

    // Frame
    free(render_data.value_buffer.b);
    
    // ---------------------- Game ----------------------

    // Explcit destruct before renderer
    background_textures.clear();
    manager_texture.clear();

    SDL_DestroyRenderer(window_renderer);
    SDL_DestroyRenderer(obs_renderer);

    SDL_DestroySurface(window_target);
    SDL_DestroySurface(obs_target);
}

// Rendering
void render_game(bool is_obs) {
    // If obs, set render to obs target
    gr.rendering_obs = is_obs;

    SDL_SetRenderDrawColor(gr.get_renderer(), 0, 0, 0, 255);
    SDL_RenderClear(gr.get_renderer());
    SDL_SetRenderDrawColor(gr.get_renderer(), 255, 255, 255, 255);

    int width = is_obs ? obs_width : window_width;
    int height = is_obs ? obs_height : window_height;

    gr.camera_scale = game_zoom * static_cast<float>(width) / static_cast<float>(obs_width);
    gr.camera_size = (Vector2){ static_cast<float>(width), static_cast<float>(height) };

    // Draw background image
    Asset_Texture* background = &background_textures[current_background_index];

    gr.render_texture(background, Vector2{ -gr.camera_size.x / gr.camera_scale * 0.5f, -gr.camera_size.y / gr.camera_scale * 0.5f }, 1.0f / background->height * gr.camera_size.y / gr.camera_scale);

    sprite_render->render(negative_z);
    mob_ai->render();
    sprite_render->render(positive_z);
    agent->render();
}

void reset() {
    c.clear_entities();

    std::uniform_real_distribution<float> spawn_dist(-1.0f, 1.0f);

    // Spawn the player (agent)
    Entity player = c.create_entity();

    c.add_component(player, Component_Transform{ .position = { spawn_dist(rng) * gr.camera_size.x / gr.camera_scale * pixels_to_unit * 0.5f, gr.camera_size.y / gr.camera_scale * pixels_to_unit * 0.5f } });
    c.add_component(player, Component_Collision{ .bounds{ -0.15f, -0.1f, 0.3f, 0.2f } });
    c.add_component(player, Component_Dynamics{});
    c.add_component(player, Component_Agent{});
    
    // Spawn the boss (mob_ai)
    Entity boss = c.create_entity();

    c.add_component(boss, Component_Transform{ .position = { 0.0f, 0.0f } });
    c.add_component(boss, Component_Collision{ .bounds{ -0.6f, -0.4f, 1.2f, 0.8f } });
    c.add_component(boss, Component_Dynamics{});
    c.add_component(boss, Component_Hazard{});
    c.add_component(boss, Component_Mob_AI{});

    // Spawn barriers
    std::uniform_int_distribution<int> num_barriers_dist(1, 4);
    std::uniform_int_distribution<int> barrier_texture_dist(0, barrier_textures.size() - 1);
    std::uniform_real_distribution<float> barrier_pos_dist_y(0.7f, 1.2f);

    int num_barriers = num_barriers_dist(rng);

    std::vector<Rectangle> barrier_collisions(num_barriers);

    for (int i = 0; i < num_barriers; i++) {
        Vector2 position = { spawn_dist(rng) * gr.camera_size.x / gr.camera_scale * pixels_to_unit * 0.5f * 0.9f, gr.camera_size.y / gr.camera_scale * pixels_to_unit * 0.5f - barrier_pos_dist_y(rng) };
        Rectangle collision = { -0.1f, -0.1f, 0.2f, 0.2f };

        Rectangle world_collision = { position.x + collision.x, position.y + collision.y, collision.width, collision.height };

        // Check existing barriers for collision
        bool collided = false;

        for (int j = 0; j < i; j++) {
            if (check_collision(world_collision, barrier_collisions[j])) {
                collided = true;

                break;
            }
        }

        if (!collided) {
            Entity barrier = c.create_entity();

            c.add_component(barrier, Component_Transform{ .position = position});
            c.add_component(barrier, Component_Collision{ .bounds = collision });
            c.add_component(barrier, Component_Hazard{});
            c.add_component(barrier, Component_Sprite{ .position = Vector2{ -0.15f, -0.15f }, .scale = 0.3f, .texture = &barrier_textures[barrier_texture_dist(rng)] });

            barrier_collisions[i] = world_collision;
        }
        else
            barrier_collisions[i] = { 0.0f, 0.0f, 0.0f, 0.0f }; // Some collision that won't trigger
    }

    // Determine background (themeing)
    std::uniform_int_distribution<int> background_dist(0, background_textures.size() - 1);

    current_background_index = background_dist(rng);

    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    current_background_offset_x = dist01(rng);
    current_background_offset_y = dist01(rng);

    // Clear before next render to remove now destroyed entities from previous episode
    sprite_render->clear_render();

    agent->reset(rng);

    mob_ai->reset(rng);
}
