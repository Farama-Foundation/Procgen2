#include "../../cenv/cenv.h"

#include <raylib.h>
#include <rlgl.h>
#include <GL/gl.h>

#include <cmath>
#include <iostream>

#include "tilemap.h"
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

const int screen_width = 800;
const int screen_height = 800;

std::mt19937 rng;
Camera2D camera;
RenderTexture2D obs_target; // Observation target

float dt = 0.017f;

// Systems
std::shared_ptr<System_Sprite_Render> sprite_render;
std::shared_ptr<System_Tilemap> tilemap;
std::shared_ptr<System_Mob_AI> mob_ai;
std::shared_ptr<System_Hazard> hazard;
std::shared_ptr<System_Goal> goal;
std::shared_ptr<System_Agent> agent;
std::shared_ptr<System_Particles> particles;

System_Tilemap::Config tilemap_config;
int current_map_theme = 0;

// Big list of different background images
std::vector<std::string> background_names {
    "assets/platform_backgrounds/alien_bg.png",
    "assets/platform_backgrounds/another_world_bg.png",
    "assets/platform_backgrounds/back_cave.png",
    "assets/platform_backgrounds/caverns.png",
    "assets/platform_backgrounds/cyberpunk_bg.png",
    "assets/platform_backgrounds/parallax_forest.png",
    "assets/platform_backgrounds/scifi_bg.png",
    "assets/platform_backgrounds/scifi2_bg.png",
    "assets/platform_backgrounds/living_tissue_bg.png",
    "assets/platform_backgrounds/airadventurelevel1.png",
    "assets/platform_backgrounds/airadventurelevel2.png",
    "assets/platform_backgrounds/airadventurelevel3.png",
    "assets/platform_backgrounds/airadventurelevel4.png",
    "assets/platform_backgrounds/cave_background.png",
    "assets/platform_backgrounds/blue_desert.png",
    "assets/platform_backgrounds/blue_grass.png",
    "assets/platform_backgrounds/blue_land.png",
    "assets/platform_backgrounds/blue_shroom.png",
    "assets/platform_backgrounds/colored_desert.png",
    "assets/platform_backgrounds/colored_grass.png",
    "assets/platform_backgrounds/colored_land.png",
    "assets/platform_backgrounds/colored_shroom.png",
    "assets/platform_backgrounds/landscape1.png",
    "assets/platform_backgrounds/landscape2.png",
    "assets/platform_backgrounds/landscape3.png",
    "assets/platform_backgrounds/landscape4.png",
    "assets/platform_backgrounds/battleback1.png",
    "assets/platform_backgrounds/battleback2.png",
    "assets/platform_backgrounds/battleback3.png",
    "assets/platform_backgrounds/battleback4.png",
    "assets/platform_backgrounds/battleback5.png",
    "assets/platform_backgrounds/battleback6.png",
    "assets/platform_backgrounds/battleback7.png",
    "assets/platform_backgrounds/battleback8.png",
    "assets/platform_backgrounds/battleback9.png",
    "assets/platform_backgrounds/battleback10.png",
    "assets/platform_backgrounds/sunrise.png",
    "assets/platform_backgrounds_2/beach1.png",
    "assets/platform_backgrounds_2/beach2.png",
    "assets/platform_backgrounds_2/beach3.png",
    "assets/platform_backgrounds_2/beach4.png",
    "assets/platform_backgrounds_2/fantasy1.png",
    "assets/platform_backgrounds_2/fantasy2.png",
    "assets/platform_backgrounds_2/fantasy3.png",
    "assets/platform_backgrounds_2/fantasy4.png",
    "assets/platform_backgrounds_2/candy1.png",
    "assets/platform_backgrounds_2/candy2.png",
    "assets/platform_backgrounds_2/candy3.png",
    "assets/platform_backgrounds_2/candy4.png"
};

std::vector<Texture2D> background_textures;

int current_background_index = 0;
float current_background_offset_x = 0.0f;

int current_agent_theme = 0;

// Forward declarations
void render_game(int width, int height); // Render to current framebuffer or render target
void reset();

void handle_log_do_nothing(int logLevel, const char *text, va_list args) {
    // Do nothing
}

int32_t cenv_get_env_version() {
    return version;
}

int32_t cenv_make(const char* render_mode, cenv_option* options, int32_t options_size) {
    // ---------------------- CEnv Interface ----------------------
    
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
    render_data.value_buffer_height = screen_height;
    render_data.value_buffer_width = screen_width;
    render_data.value_buffer_channels = 3;
    render_data.value_buffer.b = (uint8_t*)malloc(screen_width * screen_height * 3 * sizeof(uint8_t));

    unsigned int seed = time(nullptr);

    // Parse options
    for (int i = 0; i < options_size; i++) {
        std::string name(options[i].name);

        if (name == "seed") {
            assert(options[i].value_type == CENV_VALUE_TYPE_INT);

            seed = options[i].value.i;
        }
    }

    // ---------------------- Game ----------------------
    
    if (!show_log)
        SetTraceLogCallback(&handle_log_do_nothing);

    // Game
    InitWindow(screen_width, screen_height, "CoinRun");

    SetTargetFPS(0); // Uncapped

    // Create main render texture
    obs_target = LoadRenderTexture(obs_width, obs_height);

    // Seed RNG
    rng.seed(seed);

    // Initialize camera
    camera = { 0 };
    camera.zoom = 1.0f;

    // Register components
    c.register_component<Component_Transform>();
    c.register_component<Component_Collision>();
    c.register_component<Component_Dynamics>();
    c.register_component<Component_Sprite>();
    c.register_component<Component_Animation>();
    c.register_component<Component_Hazard>();
    c.register_component<Component_Goal>();
    c.register_component<Component_Mob_AI>();
    c.register_component<Component_Agent>(); // Player
    c.register_component<Component_Particles>();

    // Sprite rendering system
    sprite_render = c.register_system<System_Sprite_Render>();
    Signature sprite_render_signature;
    sprite_render_signature.set(c.get_component_type<Component_Sprite>()); // Operate only on sprites
    c.set_system_signature<System_Sprite_Render>(sprite_render_signature);

    // Tile map setup
    tilemap = c.register_system<System_Tilemap>();
    Signature tilemap_signature{ 0 }; // Operates on nothing
    c.set_system_signature<System_Tilemap>(tilemap_signature);

    tilemap->init();

    // Mob AI setup
    mob_ai = c.register_system<System_Mob_AI>();
    Signature mob_ai_signature;
    mob_ai_signature.set(c.get_component_type<Component_Mob_AI>()); // Operate only on mobs
    c.set_system_signature<System_Mob_AI>(mob_ai_signature);

    // Hazard system setup
    hazard = c.register_system<System_Hazard>();
    Signature hazard_signature;
    hazard_signature.set(c.get_component_type<Component_Hazard>()); // Operate only on hazards
    c.set_system_signature<System_Hazard>(hazard_signature);

    // Goal system setup
    goal = c.register_system<System_Goal>();
    Signature goal_signature;
    goal_signature.set(c.get_component_type<Component_Goal>()); // Operate only on goals
    c.set_system_signature<System_Goal>(goal_signature);

    // Agent system setup
    agent = c.register_system<System_Agent>();
    Signature agent_signature;
    agent_signature.set(c.get_component_type<Component_Agent>()); // Operate only on mobs
    c.set_system_signature<System_Agent>(agent_signature);

    agent->init();

    // Particle system setup
    particles = c.register_system<System_Particles>();
    Signature particles_signature;
    particles_signature.set(c.get_component_type<Component_Particles>()); // Operate only on particles
    c.set_system_signature<System_Particles>(particles_signature);

    particles->init();

    // Load backgrounds
    background_textures.resize(background_names.size());

    for (int i = 0; i < background_names.size(); i++)
        background_textures[i] = LoadTexture(background_names[i].c_str());

    // Reset spawns entities while generating map
    reset();

    return 0; // No error
}

int32_t cenv_reset(int32_t seed, cenv_option* options, int32_t options_size) {
    // Parse options
    for (int i = 0; i < options_size; i++) {
        std::string name(options[i].name);

        if (name == "seed") {
            assert(options[i].value_type == CENV_VALUE_TYPE_INT);

            rng.seed(options[i].value.i);
        }
    }

    reset();

    BeginTextureMode(obs_target);
        render_game(obs_width, obs_height);
    EndTextureMode();
    rlEnableFramebuffer(obs_target.id);
    glReadPixels(0, 0, obs_width, obs_height, GL_RGB, GL_UNSIGNED_BYTE, observation.value_buffer.b);
    rlDisableFramebuffer();

    return 0; // No error
}

int32_t cenv_step(cenv_key_value* actions, int32_t actions_size) {
    // Render and grab pixels
    BeginTextureMode(obs_target);
        render_game(obs_width, obs_height);
    EndTextureMode();
    rlEnableFramebuffer(obs_target.id);
    glReadPixels(0, 0, obs_width, obs_height, GL_RGB, GL_UNSIGNED_BYTE, observation.value_buffer.b);
    rlDisableFramebuffer();

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

    // Update systems
    mob_ai->update(dt);
    std::pair<bool, bool> result = agent->update(dt, camera, hazard, goal, action);
    sprite_render->update(dt);

    step_data.reward.f = result.second * 10.0f;

    step_data.terminated = !result.first || result.second;
    step_data.truncated = false;

    return 0; // No error
}

int32_t cenv_render() {
    // Render and grab pixels
    BeginDrawing();
        render_game(screen_width, screen_height);
        glReadPixels(0, 0, screen_width, screen_height, GL_RGB, GL_UNSIGNED_BYTE, render_data.value_buffer.b);
    EndDrawing();

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

    UnloadRenderTexture(obs_target);

    // Unload background textures
    for (int i = 0; i < background_textures.size(); i++)
        UnloadTexture(background_textures[i]);

    CloseWindow();
}

// Rendering
void render_game(int width, int height) {
    camera.zoom = static_cast<float>(width) / static_cast<float>(screen_width);
    camera.offset = (Vector2){ width * 0.5f, height * 0.5f };
    float zoom_inv = 1.0f / camera.zoom;

    // Render here
    BeginMode2D(camera);

        ClearBackground(BLACK);

        // Draw background image
        Texture2D background = background_textures[current_background_index];

        float background_aspect = static_cast<float>(background.width) / static_cast<float>(background.height);
        float extra_width = background_aspect - 1.0f; // 1 for game world aspect, which is 64x64 tiles
        
        DrawTextureEx(background, Vector2{ -current_background_offset_x * extra_width, 0.0f }, 0.0f, 64.0f * unit_to_pixels / background.height, WHITE);

        Rectangle camera_aabb;
        camera_aabb.x = (camera.target.x - zoom_inv * width * 0.5f) * pixels_to_unit;
        camera_aabb.y = (camera.target.y - zoom_inv * height * 0.5f) * pixels_to_unit;
        camera_aabb.width = width * zoom_inv * pixels_to_unit;
        camera_aabb.height = height * zoom_inv * pixels_to_unit;

        sprite_render->render(camera_aabb, negative_z);
        tilemap->render(camera_aabb, current_map_theme);
        particles->render(camera_aabb);
        sprite_render->render(camera_aabb, positive_z);
        agent->render(current_agent_theme);

    EndMode2D();
}

void reset() {
    c.destroy_all_entities();

    tilemap->regenerate(rng, tilemap_config);

    // Determine background (themeing)
    std::uniform_int_distribution<int> background_dist(0, background_textures.size() - 1);

    current_background_index = background_dist(rng);

    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);

    current_background_offset_x = dist01(rng);

    // Spawn the player (agent)
    Entity e = c.create_entity();

    Vector2 pos{ 1.5f, tilemap->get_height() - 1 - 1.0f };

    c.add_component(e, Component_Transform{ .position{ pos } });
    c.add_component(e, Component_Collision{ .bounds{ -0.5f, -1.0f, 1.0f, 1.0f } });
    c.add_component(e, Component_Dynamics{});
    c.add_component(e, Component_Agent{});

    // Determine themes
    std::uniform_int_distribution<int> agent_theme_dist(0, agent_themes.size() - 1);

    current_agent_theme = agent_theme_dist(rng);

    std::uniform_int_distribution<int> map_theme_dist(0, wall_themes.size() - 1);

    current_map_theme = map_theme_dist(rng);
}
