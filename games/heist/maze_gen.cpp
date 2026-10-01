#include "maze_gen.h"

#include <algorithm>

struct Wall {
    int x1, y1, x2, y2;
};

int MazeGen::randn(std::mt19937& rng, int n) {
    if (n <= 1) return 0;
    return std::uniform_int_distribution<int>(0, n - 1)(rng);
}

int MazeGen::get(int x, int y) const {
    if (x < 0 || y < 0 || x >= array_dim || y >= array_dim)
        return WALL_OBJ;
    return grid[to_index(x, y)];
}

void MazeGen::set(int x, int y, int v) {
    if (x < 0 || y < 0 || x >= array_dim || y >= array_dim) return;
    grid[to_index(x, y)] = v;
}

int MazeGen::lookup(int x, int y) const {
    return cell_sets_idxs[maze_dim * y + x];
}

void MazeGen::set_free_cell(int x, int y) {
    set(x + MAZE_OFFSET, y + MAZE_OFFSET, SPACE);
    int cell = maze_dim * y + x;
    if (free_cell_set.find(cell) == free_cell_set.end()) {
        free_cells[num_free_cells] = cell;
        free_cell_set.insert(cell);
        num_free_cells += 1;
    }
}

int MazeGen::get_obj(int idx) const {
    int x = idx % array_dim;
    int y = idx / array_dim;
    if (x <= 0 || x >= array_dim - 1) return INVALID_OBJ;
    if (y <= 0 || y >= array_dim - 1) return INVALID_OBJ;
    return get(x, y);
}

void MazeGen::get_neighbors(int idx, int type, std::vector<int>& neighbors) const {
    int x = idx % array_dim;
    int y = idx / array_dim;
    neighbors.clear();
    const int dxs[4] = {-1, 1, 0, 0};
    const int dys[4] = {0, 0, -1, 1};
    for (int k = 0; k < 4; k++) {
        int n_idx = to_index(x + dxs[k], y + dys[k]);
        if (get_obj(n_idx) == type)
            neighbors.push_back(n_idx);
    }
}

int MazeGen::expand_to_type(std::set<int>& s0, std::set<int>& s1, int type) {
    std::set<int> curr = s0;
    std::vector<int> target_elems, adj_space;

    while (!curr.empty()) {
        std::set<int> next;
        for (int elem : curr) {
            get_neighbors(elem, type, target_elems);
            get_neighbors(elem, SPACE, adj_space);
            for (int j : adj_space) {
                if (s0.find(j) == s0.end() && s1.find(j) == s1.end()) {
                    next.insert(j);
                    s1.insert(j);
                }
            }
            if (!target_elems.empty())
                return target_elems[0];
        }
        curr = next;
    }
    return -1;
}

std::vector<int> MazeGen::filter_cells(int type) const {
    std::vector<int> found;
    for (int i = 0; i < array_dim * array_dim; i++) {
        if (get_obj(i) == type)
            found.push_back(i);
    }
    return found;
}

void MazeGen::generate_maze(int dim, std::mt19937& rng) {
    maze_dim = dim;
    array_dim = maze_dim + 2;
    cell_sets.assign(array_dim * array_dim, {});
    cell_sets_idxs.assign(array_dim * array_dim, 0);
    free_cells.assign(array_dim * array_dim, 0);
    grid.assign(array_dim * array_dim, WALL_OBJ);

    set(MAZE_OFFSET, MAZE_OFFSET, SPACE);

    std::vector<Wall> walls;
    num_free_cells = 0;
    free_cell_set.clear();

    std::set<int>* s0 = &cell_sets[0];
    s0->clear();
    s0->insert(0);
    cell_sets_idxs[0] = 0;

    for (int i = 1; i < maze_dim * maze_dim; i++) {
        cell_sets[i].clear();
        cell_sets[i].insert(i);
        cell_sets_idxs[i] = i;
    }

    for (int i = 1; i < maze_dim; i += 2) {
        for (int j = 0; j < maze_dim; j += 2) {
            if (i > 0 && i < maze_dim - 1)
                walls.push_back({i - 1, j, i + 1, j});
        }
    }
    for (int i = 0; i < maze_dim; i += 2) {
        for (int j = 1; j < maze_dim; j += 2) {
            if (j > 0 && j < maze_dim - 1)
                walls.push_back({i, j - 1, i, j + 1});
        }
    }

    while (!walls.empty()) {
        int n = randn(rng, (int)walls.size());
        Wall wall = walls[n];

        int s0_idx = lookup(wall.x1, wall.y1);
        s0 = &cell_sets[s0_idx];
        int s1_idx = lookup(wall.x2, wall.y2);
        std::set<int>* s1 = &cell_sets[s1_idx];

        int x0 = (wall.x1 + wall.x2) / 2;
        int y0 = (wall.y1 + wall.y2) / 2;
        int center = maze_dim * y0 + x0;

        bool can_remove =
            (get(x0 + MAZE_OFFSET, y0 + MAZE_OFFSET) == WALL_OBJ) &&
            (s0_idx != s1_idx);

        if (can_remove) {
            set_free_cell(wall.x1, wall.y1);
            set_free_cell(x0, y0);
            set_free_cell(wall.x2, wall.y2);

            s1->insert(s0->begin(), s0->end());
            s1->insert(center);
            for (int it : *s1)
                cell_sets_idxs[it] = s1_idx;
        }

        walls.erase(walls.begin() + n);
    }
}

void MazeGen::generate_maze_with_doors(int dim, int num_doors, std::mt19937& rng) {
    generate_maze(dim, rng);

    std::vector<int> forks;
    std::vector<int> adj_space, adj_wall;
    for (int i = 0; i < array_dim * array_dim; i++) {
        if (get_obj(i) == SPACE) {
            get_neighbors(i, SPACE, adj_space);
            get_neighbors(i, WALL_OBJ, adj_wall);
            if (adj_space.size() > 2)
                forks.push_back(i);
        }
    }

    std::vector<int> chosen;
    int take = std::min(num_doors, (int)forks.size());
    std::vector<int> order = forks;
    for (int i = (int)order.size() - 1; i > 0; i--)
        std::swap(order[i], order[randn(rng, i + 1)]);
    chosen.assign(order.begin(), order.begin() + take);
    num_doors = (int)chosen.size();

    for (int i : chosen)
        grid[i] = DOOR_OBJ;

    int agent_cell;
    {
        std::vector<int> space_cells = filter_cells(SPACE);
        std::vector<int> door_neighbors;
        if (space_cells.empty())
            return;
        int guard = 0;
        do {
            agent_cell = space_cells[randn(rng, (int)space_cells.size())];
            door_neighbors.clear();
            get_neighbors(agent_cell, DOOR_OBJ, door_neighbors);
            guard++;
        } while (!door_neighbors.empty() && guard < 200);
        grid[agent_cell] = AGENT_OBJ;
    }

    std::set<int> s0;
    s0.insert(agent_cell);

    for (int door_num = 0; door_num < num_doors + 1; door_num++) {
        std::set<int> s1;
        int found_door = -1;

        if (door_num < num_doors) {
            found_door = expand_to_type(s0, s1, DOOR_OBJ);
            if (found_door >= 0)
                grid[found_door] = DOOR_OBJ + door_num + 1;
            s0.insert(s1.begin(), s1.end());
        }

        expand_to_type(s0, s1, -999);

        std::vector<int> space_cells(s1.begin(), s1.end());
        if (space_cells.empty())
            continue;

        int key_cell = space_cells[randn(rng, (int)space_cells.size())];
        grid[key_cell] = (door_num == num_doors) ? EXIT_OBJ : (KEY_OBJ + door_num + 1);

        s0.insert(s1.begin(), s1.end());
        if (found_door >= 0)
            s0.insert(found_door);
    }
}
