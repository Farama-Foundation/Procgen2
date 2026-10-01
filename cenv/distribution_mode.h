#pragma once

#include "cenv.h"

#include <cstring>

// Matches openai/procgen DistributionMode in game.h (exploration is Python-only).
enum {
    DIST_EASY = 0,
    DIST_HARD = 1,
    DIST_EXTREME = 2,
    DIST_MEMORY = 10,
    DIST_EXPLORATION = 20
};

inline int cenv_parse_distribution_mode(cenv_option* options, int32_t options_size, int fallback = DIST_HARD) {
    for (int i = 0; i < options_size; i++) {
        if (options[i].name != nullptr && std::strcmp(options[i].name, "distribution_mode") == 0) {
            if (options[i].value_type == CENV_VALUE_TYPE_INT)
                return options[i].value.i;
        }
    }
    return fallback;
}
