#pragma once

#include <vector>
#include <set>
#include <random>

const int MAZE_OFFSET = 1;

const int INVALID_OBJ = -1;
const int SPACE       = 100;
const int WALL_OBJ    = 51;
const int EXIT_OBJ    = 52;
const int AGENT_OBJ   = 53;
const int DOOR_OBJ    = 200;
const int KEY_OBJ     = 300;

class MazeGen {
public:
    int maze_dim = 0;
    int array_dim = 0;
    std::vector<int> grid;  // array_dim × array_dim, index y * array_dim + x

    MazeGen() = default;
    void generate_maze(int dim, std::mt19937& rng);
    void generate_maze_with_doors(int dim, int num_doors, std::mt19937& rng);

    int get(int x, int y) const;
    void set(int x, int y, int v);
    int to_index(int x, int y) const { return y * array_dim + x; }

private:
    int num_free_cells = 0;
    std::vector<std::set<int>> cell_sets;
    std::vector<int> cell_sets_idxs;
    std::set<int> free_cell_set;
    std::vector<int> free_cells;

    int randn(std::mt19937& rng, int n);
    int lookup(int x, int y) const;
    void set_free_cell(int x, int y);
    int get_obj(int idx) const;
    void get_neighbors(int idx, int type, std::vector<int>& neighbors) const;
    int expand_to_type(std::set<int>& s0, std::set<int>& s1, int type);
    std::vector<int> filter_cells(int type) const;
};
