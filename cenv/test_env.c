#include "cenv.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// Define globals from cenv
cenv_make_data make_data;
cenv_reset_data reset_data;
cenv_step_data step_data;
cenv_render_data render_data;

// Shared value between different datas (optional)
cenv_key_value observation;

float t; // Timer

int32_t cenv_get_env_version() {
    return 123;
}

int32_t cenv_make(char* render_mode, cenv_option* options, int32_t options_size) {
    // Allocate make data
    make_data.observation_spaces_size = 1;
    make_data.observation_spaces = (cenv_key_value*)malloc(sizeof(cenv_key_value));

    make_data.observation_spaces[0].key = "obs1";
    make_data.observation_spaces[0].value_type = CENV_SPACE_TYPE_BOX;
    make_data.observation_spaces[0].value_buffer_size = 20; // Low and high, both 10 in size

    make_data.observation_spaces[0].value_buffer.f = (float*)malloc(make_data.observation_spaces[0].value_buffer_size * sizeof(float));

    // Low
    for (int i = 0; i < 10; i++)
        make_data.observation_spaces[0].value_buffer.f[i] = -1.0f;

    // High
    for (int i = 10; i < 20; i++)
        make_data.observation_spaces[0].value_buffer.f[i] = 1.0f;

    make_data.action_spaces_size = 1;
    make_data.action_spaces = (cenv_key_value*)malloc(sizeof(cenv_key_value));

    make_data.action_spaces[0].key = "act1";
    make_data.action_spaces[0].value_type = CENV_SPACE_TYPE_MULTI_DISCRETE;
    make_data.action_spaces[0].value_buffer_size = 1;

    make_data.action_spaces[0].value_buffer.i = (int32_t*)malloc(sizeof(int32_t));
    make_data.action_spaces[0].value_buffer.i[0] = 10;

    // Allocate observations once and re-use (doesn't resize dynamically)
    observation.key = "obs";
    observation.value_type = CENV_VALUE_TYPE_FLOAT;
    observation.value_buffer_size = 10;
    observation.value_buffer.f = (float*)malloc(10 * sizeof(float));

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
    render_data.value_buffer_height = 8;
    render_data.value_buffer_width = 8;
    render_data.value_buffer_channels = 3;
    render_data.value_buffer.b = (uint8_t*)malloc(8 * 8 * 3 * sizeof(uint8_t));

    // Game
    t = 0.0f;

    return 0; // No error
}

int32_t cenv_reset(int32_t seed, cenv_option* options, int32_t options_size) {
    t = 0.0f;

    for (int i = 0; i < observation.value_buffer_size; i++)
        observation.value_buffer.f[i] = cosf(t + 0.5f * i);

    return 0; // No error
}

int32_t cenv_step(cenv_key_value* actions, int32_t actions_size) {
    step_data.reward.f = sinf(t);

    for (int i = 0; i < observation.value_buffer_size; i++)
        observation.value_buffer.f[i] = cosf(t + 0.5f * i);

    t += 0.25f;

    step_data.terminated = t >= 10.0f;

    return 0; // No error
}

int32_t cenv_render() {
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            render_data.value_buffer.b[0 + 3 * (x + 8 * y)] = 64;
            render_data.value_buffer.b[1 + 3 * (x + 8 * y)] = 64;
            render_data.value_buffer.b[2 + 3 * (x + 8 * y)] = 64;
        }

    return 0; // No error
}

void cenv_close() {
    // Dealloc make data
    for (int i = 0; i < make_data.observation_spaces_size; i++)
        free(make_data.observation_spaces[i].value_buffer.f);

    free(make_data.observation_spaces);

    for (int i = 0; i < make_data.action_spaces_size; i++)
        free(make_data.action_spaces[i].value_buffer.i);

    free(make_data.action_spaces);

    // Observations
    free(observation.value_buffer.f);

    // Frame
    free(render_data.value_buffer.b);
}
