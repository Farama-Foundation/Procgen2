#pragma once

#include "helpers.h"

#include <cmath>
#include <algorithm>
#include <random>
#include <functional>
#include <unordered_set>
#include <array>

class Maze_Generator {
public:
    int maze_width = 0;
    int maze_height = 0;
    int array_width = 0;
    int array_height = 0;

    // Main grid
    std::vector<int> grid;

    // Sets
    int num_free_cells;
    std::vector<std::unordered_set<int>> cell_sets;
    std::vector<int> cell_sets_indices;
    std::unordered_set<int> free_cell_set;
    std::vector<int> free_cells;

    void set_free_cell(int x, int y);

    // Row major
    int get_index(int x, int y) const {
        return y + array_height * x;
    }

    std::pair<int, int> get_position(int index) {
        return std::make_pair(index / array_height, index % array_height);
    }

    int get(int x, int y) const {
        if (x < 0 || y < 0 || x >= array_width || y >= array_height)
            return 1; // Out of bounds is wall

        return grid[get_index(x, y)];
    }

    // Van Neumann neighborhood
    std::array<int, 4> get_neighbor_indices(int x, int y) const;

    // Generators
    void generate_maze(int maze_width, int maze_height, std::mt19937 &rng);
    void generate_maze_no_dead_ends(int maze_width, int maze_height, std::mt19937 &rng);
};
