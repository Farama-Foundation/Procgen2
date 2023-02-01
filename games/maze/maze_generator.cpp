#include "maze_generator.h"

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
    grid[get_index(x + maze_offset, y + maze_offset)] = EMPTY_CELL; // Space

    int cell_index = y + maze_height * x;

    if (free_cell_set.find(cell_index) == free_cell_set.end()) {
        free_cells[num_free_cells] = cell_index;
        free_cell_set.insert(cell_index);

        num_free_cells++;
    }
}

// Classical path compression
// int Maze_Generator::find_id_of_set(int cell) {
//     int parent = cell_sets_indices[cell];
//     if (parent == cell) {
//         return cell;
//     } else {
//         cell_sets_indices[cell] = find_id_of_set(parent);
//         return cell_sets_indices[cell];
//     }
// }

// Classical path compression -- iterative version
int Maze_Generator::find_id_of_set(int cell) {
    int id = cell;
    while (cell_sets_indices[id] != id) {
        id = cell_sets_indices[id];
    }
    while (cell_sets_indices[cell] != id) {
        int parent = cell_sets_indices[cell];
        cell_sets_indices[cell] = id;
        cell = parent;
    }
    return id;
}

// Tarjan-van Leeuwen path splitting
// int Maze_Generator::find_id_of_set(int cell) {
//     int curr_cell = cell;
//     while (cell_sets_indices[curr_cell] != curr_cell) {
//         int parent = cell_sets_indices[curr_cell];
//         cell_sets_indices[curr_cell] = cell_sets_indices[parent];
//         curr_cell = parent;
//     }
//     return curr_cell;
// }

// Tarjan-van Leeuwen path halving
// int Maze_Generator::find_id_of_set(int cell) {
//     int curr_cell = cell;
//     while (cell_sets_indices[curr_cell] != curr_cell) {
//         curr_cell = cell_sets_indices[curr_cell] = cell_sets_indices[cell_sets_indices[curr_cell]];
//     }
//     return curr_cell;
// }

void Maze_Generator::generate_maze(int maze_width, int maze_height, std::mt19937 &rng) {
    this->maze_width = maze_width;
    this->maze_height = maze_height;
    array_width = maze_width + 2*maze_offset; // Padding
    array_height = maze_height + 2*maze_offset; // Padding

    cell_sets_ranks.resize(array_width * array_height);
    cell_sets_indices.resize(array_width * array_height);
    free_cells.resize(array_width * array_height);
    grid.clear();
    grid.resize(array_width * array_height);

    // Clear to wall
    std::fill(grid.begin(), grid.end(), WALL_CELL); // Wall

    // Corner
    grid[get_index(maze_offset, maze_offset)] = EMPTY_CELL;

    std::vector<Wall> walls;

    // Clear
    num_free_cells = 0;
    free_cell_set.clear();

    cell_sets_indices[0] = 0;
    cell_sets_ranks[0] = 0;

    int maze_size = maze_width * maze_height;

    for (int i = 1; i < maze_size; i++) {
        cell_sets_indices[i] = i;
        cell_sets_ranks[i] = 0;
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

        int s0_index = find_id_of_set(wall.y1 + maze_height * wall.x1);
        int s1_index = find_id_of_set(wall.y2 + maze_height * wall.x2);

        int x0 = (wall.x1 + wall.x2) / 2;
        int y0 = (wall.y1 + wall.y2) / 2;
        int center = y0 + maze_height * x0;

        bool can_remove = (grid[get_index(x0 + maze_offset, y0 + maze_offset)] == WALL_CELL) && (s0_index != s1_index);

        if (can_remove) {
            set_free_cell(wall.x1, wall.y1);
            set_free_cell(x0, y0);
            set_free_cell(wall.x2, wall.y2);

            if (cell_sets_ranks[s0_index] > cell_sets_ranks[s1_index]) {
                cell_sets_indices[s1_index] = s0_index;
            } else {
                cell_sets_indices[s0_index] = s1_index;
                if (cell_sets_ranks[s0_index] == cell_sets_ranks[s1_index]) {
                    cell_sets_ranks[s1_index]++;
                }
            }

            // else if (cell_sets_ranks[cell0] == cell_sets_ranks[cell1]) {
            //     cell_sets_indices[cell1] = cell0;
            //     cell_sets_ranks[cell0]++;
            // } else {
            //     cell_sets_indices[cell0] = cell1;
            // }

            // for (std::unordered_set<int>::const_iterator it = s1->begin(); it != s1->end(); it++)
            //     cell_sets_indices[*it] = s1_index;
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

void Maze_Generator::place_object(int obj_type, std::mt19937 &rng) {
    std::uniform_int_distribution<int> n_dist(0, num_free_cells - 1);
    int free_cell_idx = n_dist(rng);
    // std::uniform_int_distribution<int> n_dist(1, maze_width*maze_height - 1);
    // int obj_cell = n_dist(rng);

    while (free_cells[free_cell_idx] == INVALID_CELL || free_cells[free_cell_idx] == START_CELL) {
        free_cell_idx = n_dist(rng);
    }
    // while (grid[obj_cell] != EMPTY_CELL) {
    //     obj_cell = n_dist(rng);
    // }

    int obj_cell = free_cells[free_cell_idx];
    free_cells[free_cell_idx] = INVALID_CELL;

    grid[obj_cell] = obj_type;
    grid[get_index(obj_cell / maze_height + maze_offset, obj_cell % maze_width + maze_offset)] = obj_type;
}
