#include "../../cenv/cenv.h"

#include <raylib.h>
#include <rlgl.h>

#include <cmath>
#include <iostream>

const int version = 100;
const bool show_log = true;

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

// Forward declarations
void render_game();
void copy_render_to_obs();

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

    // ---------------------- Game ----------------------
    
    if (!show_log)
        SetTraceLogCallback(&handle_log_do_nothing);

    // Game
    InitWindow(screen_width, screen_height, "CoinRun");

    SetTargetFPS(0); // Uncapped

    return 0; // No error
}

int32_t cenv_reset(int32_t seed, cenv_option* options, int32_t options_size) {
    render_game();

    copy_render_to_obs();

    return 0; // No error
}

int32_t cenv_step(cenv_key_value* actions, int32_t actions_size) {
    render_game();

    copy_render_to_obs();

    // Set observation from last render
    step_data.reward.f = 0.0f;

    step_data.terminated = false;

    return 0; // No error
}

int32_t cenv_render() {
    // Copy pixels
    Image screen = LoadImageFromScreen();

    ImageFormat(&screen, PIXELFORMAT_UNCOMPRESSED_R8G8B8);

    uint8_t* pixels = (uint8_t*)screen.data;

    // Reformat here if needed
    for (int x = 0; x < screen_width; x++)
        for (int y = 0; y < screen_height; y++) {
            render_data.value_buffer.b[0 + 3 * (y + screen_height * x)] = pixels[0 + 3 * (y + screen_height * x)];
            render_data.value_buffer.b[1 + 3 * (y + screen_height * x)] = pixels[1 + 3 * (y + screen_height * x)];
            render_data.value_buffer.b[2 + 3 * (y + screen_height * x)] = pixels[2 + 3 * (y + screen_height * x)];
        }

    UnloadImage(screen);

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
    
    CloseWindow();
}

// Rendering
void render_game() {
    // Render here
    BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Congrats! You created your first window!", 95, 100, 20, LIGHTGRAY);
    EndDrawing();
}

void copy_render_to_obs() {
    Image screen = LoadImageFromScreen();

    ImageFormat(&screen, PIXELFORMAT_UNCOMPRESSED_R8G8B8);

    Image obsImg = ImageCopy(screen);

    ImageResize(&obsImg, obs_width, obs_height);

    uint8_t* pixels = (uint8_t*)obsImg.data;

    // Set observation, reformat if needed
    for (int x = 0; x < obs_width; x++)
        for (int y = 0; y < obs_height; y++) {
            observation.value_buffer.b[0 + 3 * (y + obs_height * x)] = pixels[0 + 3 * (y + obs_height * x)];
            observation.value_buffer.b[1 + 3 * (y + obs_height * x)] = pixels[1 + 3 * (y + obs_height * x)];
            observation.value_buffer.b[2 + 3 * (y + obs_height * x)] = pixels[2 + 3 * (y + obs_height * x)];
        }

    UnloadImage(obsImg);
    UnloadImage(screen);
}
