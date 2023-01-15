#pragma once

#include "helpers.h"

#include <cmath>
#include <algorithm>
#include <random>
#include <functional>
#include <unordered_set>
#include <queue>

class Room_Generator {
public:
    int grid_width = 0;
    int grid_height = 0;

    std::vector<int> grid;

    // Row major
    int get_index(int x, int y) const {
        return y + grid_height * x;
    }

    std::pair<int, int> get_position(int index) {
        return std::make_pair(index / grid_height, index % grid_height);
    }

    void set(int x, int y, int id) {
        if (x < 0 || y < 0 || x >= grid_width || y >= grid_height)
            return; // Out of bounds

        grid[get_index(x, y)] = id;
    }

    int get(int x, int y) const {
        if (x < 0 || y < 0 || x >= grid_width || y >= grid_height)
            return 1; // Out of bounds

        return grid[get_index(x, y)];
    }

    void build_room(int index, std::unordered_set<int> &room);
    int count_neighbors(int index, int type);

    void init(int grid_width, int grid_height) {
        this->grid_width = grid_width;
        this->grid_height = grid_height;

        grid.clear();
        grid.resize(grid_width * grid_height, 0);
    }

    void update();

    void find_path(int src, int dst, std::vector<int> &path);
    void find_best_room(std::unordered_set<int> &best_room);
    void expand_room(std::unordered_set<int> &set, int n);
};
