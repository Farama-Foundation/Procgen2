#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(_WIN32)
#if defined(__GNUC__)
#define CENV_API __attribute__((__dllexport__))
#else
#define CENV_API __declspec(dllexport)
#endif
#else
#if defined(__GNUC__)
#define CENV_API __attribute__((__visibility__("default")))
#else
#define CENV_API
#endif
#endif

#define CENV_VERSION 1

// Poissible value types
typedef enum {
    // Primitive values
    CENV_VALUE_TYPE_INT = 0,
    CENV_VALUE_TYPE_FLOAT = 1,
    CENV_VALUE_TYPE_DOUBLE = 2,
    CENV_VALUE_TYPE_BYTE = 3,

    // Spaces
    CENV_SPACE_TYPE_BOX = 4,
    CENV_SPACE_TYPE_MULTI_DISCRETE = 5
} cenv_value_type;

// Generic value type
typedef union {
    int32_t i;
    float f;
    double d;
    uint8_t b;
} cenv_value;

// Generic buffer type
typedef union {
    int32_t* i;
    float* f;
    double* d;
    uint8_t* b;
} cenv_value_buffer;

typedef struct {
    // Key is a short string
    char* key;

    // Tag and union
    cenv_value_type value_type;
    int32_t value_buffer_size;
    cenv_value_buffer value_buffer;
} cenv_key_value;

// Options for setup
typedef struct {
    char* name;

    // Tag and union
    cenv_value_type value_type;
    cenv_value value;
} cenv_option;

// Returned by make
typedef struct {
    int32_t observation_spaces_size;
    cenv_key_value* observation_spaces;

    int32_t action_spaces_size;
    cenv_key_value* action_spaces;
} cenv_make_data;

// Returned by reset
typedef struct {
    int32_t observations_size;
    cenv_key_value* observations;

    int32_t infos_size;
    cenv_key_value* infos;
} cenv_reset_data;

// Returned by step
typedef struct {
    int32_t observations_size;
    cenv_key_value* observations;

    cenv_value reward;
    bool terminated;
    bool truncated;

    int32_t infos_size;
    cenv_key_value* infos;
} cenv_step_data;

// Returned by render
typedef struct {
    // Type of image
    cenv_value_type value_type;

    // Dimensions of image
    int32_t value_buffer_width;
    int32_t value_buffer_height;
    int32_t value_buffer_channels;
    
    // Image buffer
    cenv_value_buffer value_buffer; // Size height * width * channels, addressed like: channel_index + channels * (x + width * y)
} cenv_render_data;

// C ENV DEVELOPERS: USE THESE IN YOUR ENV!
CENV_API extern cenv_make_data make_data;
CENV_API extern cenv_reset_data reset_data;
CENV_API extern cenv_step_data step_data;
CENV_API extern cenv_render_data render_data;

// C ENV DEVELOPERS: IMPLEMENT THESE IN YOUR ENV!
CENV_API int32_t cenv_get_env_version(); // Version of environment
CENV_API int32_t cenv_make(char* render_mode, cenv_option* options, int32_t options_size); // Make the environment
CENV_API int32_t cenv_reset(int32_t seed, cenv_option* options, int32_t options_size); // Reset the environment
CENV_API int32_t cenv_step(cenv_key_value* actions, int32_t actions_size); // Step (update) the environment
CENV_API int32_t cenv_render(); // Render the environment to a frame
CENV_API void cenv_close(); // Close (delete) the environment (shutdown)

#ifdef __cplusplus
}
#endif
