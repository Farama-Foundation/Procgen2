#include "maze_generator.h"

const int maze_offset = 1;

struct Wall {
    int x1;
    int y1;
    int x2;
    int y2;
};

std::vector<int> Maze_Generator::get_neighbor_indices(int x, int y) const {
    std::vector<int> neighbors;

    for (int dx = -1; dx <= 1; dx += 2) {
        int nx = x + dx;

        if (nx < 0 || nx >= array_width)
            continue;

        neighbors.push_back(get_index(nx, y));
    }

    for (int dy = -1; dy <= 1; dy += 2) {
        int ny = y + dy;

        if (ny < 0 || ny >= array_width)
            continue;

        neighbors.push_back(get_index(x, ny));
    }

    return neighbors;
}

void Maze_Generator::set_free_cell(int x, int y) {
    grid[get_index(x + maze_offset, y + maze_offset)] = 0; // Space

    int cell_index = y + maze_height * x;

    if (free_cell_set.find(cell_index) == free_cell_set.end()) {
        free_cells[num_free_cells] = cell_index;
        free_cell_set.insert(cell_index);

        num_free_cells++;
    }
}

void Maze_Generator::generate_maze(int maze_width, int maze_height, std::mt19937 &rng) {
    this->maze_width = maze_width;
    this->maze_height = maze_height;
    array_width = maze_width + 2; // Padding
    array_height = maze_height + 2; // Padding

    cell_sets.resize(array_width * array_height);
    cell_sets_indices.resize(array_width * array_height);
    free_cells.resize(array_width * array_height);
    grid.resize(array_width * array_height);

    // Clear to wall
    std::fill(grid.begin(), grid.end(), 1); // Wall

    // Corner
    grid[get_index(maze_offset, maze_offset)] = 0;

    std::vector<Wall> walls;

    // Clear
    num_free_cells = 0;
    free_cell_set.clear();

    std::unordered_set<int> &s0 = cell_sets[0];
    s0.clear();
    s0.insert(0);
    cell_sets_indices[0] = 0;

    int maze_size = maze_width * maze_height;

    for (int i = 1; i < maze_size; i++) {
        std::unordered_set<int> &s1 = cell_sets[i];
        s1.clear();
        s1.insert(i);
        cell_sets_indices[i] = i;
    }

    for (int i = 1; i < maze_width; i += 2) {
        for (int j = 0; j < maze_height; j += 2) {
            if (i > 0 && i < maze_width - 1)
                walls.push_back({ i - 1, j, i + 1, j });
        }
    }

    for (int i = 0; i < maze_width; i += 2) {
        for (int j = 1; j < maze_height; j += 2) {
            if (j > 0 && j < maze_height - 1)
                walls.push_back({ i, j - 1, i, j + 1 });
        }
    }

    while (walls.size() > 0) {
        std::uniform_int_distribution<int> n_dist(0, walls.size() - 1);

        int n = n_dist(rng);

        Wall wall = walls[n];

        int s0_index = cell_sets_indices[wall.y1 + maze_height * wall.x1];
        s0 = cell_sets[s0_index];
        int s1_index = cell_sets_indices[wall.y2 + maze_height * wall.x2];
        std::unordered_set<int> &s1 = cell_sets[s1_index];

        int x0 = (wall.x1 + wall.x2) / 2;
        int y0 = (wall.y1 + wall.y2) / 2;
        int center = y0 + maze_height * x0;

        bool can_remove = (grid[get_index(x0 + maze_offset, y0 + maze_offset)] == 1) && (s0_index != s1_index);

        if (can_remove) {
            set_free_cell(wall.x1, wall.y1);
            set_free_cell(x0, y0);
            set_free_cell(wall.x2, wall.y2);

            s1.insert(s0.begin(), s0.end());
            s1.insert(center);

            for (std::unordered_set<int>::const_iterator it = s1.begin(); it != s1.end(); it++)
                cell_sets_indices[*it] = s1_index;
        }

        walls.erase(walls.begin() + n);
    }
}

void Maze_Generator::generate_maze_no_dead_ends(int maze_width, int maze_height, std::mt19937 &rng) {
    generate_maze(maze_width, maze_height, rng);

    std::vector<int> adj_space;
    std::vector<int> adj_wall;

    int array_size = array_width * array_height;

    for (int i = 0; i < array_size; i++) {
        if (grid[i] == 0) { // Space
            std::vector<int> neighbors = get_neighbor_indices(i / array_height, i % array_height);

            int num_adjacent_spaces = 0;

            for (int n = 0; n < neighbors.size(); n++) {
                if (grid[neighbors[n]] == 0) // Space
                    num_adjacent_spaces++;
            }

            if (num_adjacent_spaces == 1) {
                int num_adjacent_walls = 0;

                for (int n = 0; n < neighbors.size(); n++) {
                    if (grid[neighbors[n]] == 1) // Wall
                        num_adjacent_walls++;
                }

                if (num_adjacent_walls > 0) {
                    std::uniform_int_distribution<int> n_dist(0, num_adjacent_walls - 1);

                    int n = n_dist(rng);

                    while (grid[neighbors[n]] != 1)
                        n++;

                    grid[neighbors[n]] = 0; // Set space randomly
                }
            }
        }
    }
}
