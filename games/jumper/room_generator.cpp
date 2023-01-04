#include "room_generator.h"
#include <iostream>

int Room_Generator::count_neighbors(int index, int type) {
    int x = index / grid_height;
    int y = index % grid_height;

    int n = 0;

    // Moore neighborhood
    for (int i = -1; i <= 1; i++) {
        for (int j = -1; j <= 1; j++) {
            if (get(x + i, y + j) == type)
                n++;
        }
    }

    return n;
}

void Room_Generator::update() {
    // Update cellular automata
    int grid_size = grid_width * grid_height;

    // Temporary double-buffer
    std::vector<int> next_cells(grid_size);

    for (int i = 0; i < grid_size; i++) {
        if (count_neighbors(i, 1) >= 5) // More than 5 walls
            next_cells[i] = 1; // Add wall
        else
            next_cells[i] = 0; // Add space
    }

    grid = next_cells;
}

void Room_Generator::build_room(int index, std::unordered_set<int> &room) {
    std::queue<int> current;

    if (grid[index] != 0) // Not space
        return;

    current.push(index);

    while (current.size() > 0) {
        int current_index = current.front();

        current.pop();

        if (grid[current_index] != 0) // Not space
            continue;

        int x = current_index / grid_height;
        int y = current_index % grid_height;

        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if ((i == 0 || j == 0) && (i + j != 0)) {
                    int next_index = get_index(x + i, y + j);

                    if (room.find(next_index) == room.end() && grid[next_index] == 0) {
                        current.push(next_index);
                        room.insert(next_index);
                    }
                }
            }
        }
    }
}

void Room_Generator::find_path(int src, int dst, std::vector<int> &path) {
    std::unordered_set<int> covered;
    std::vector<int> expanded;
    std::vector<int> parents;

    if (grid[src] != 0) // Not space
        return;

    expanded.push_back(src);
    parents.push_back(-1);

    int search_index = 0;

    while (search_index < expanded.size()) {
        int current_index = expanded[search_index];

        if (current_index == dst)
            break;

        if (grid[current_index] != 0) // Not space
            continue;

        int x = current_index / grid_height;
        int y = current_index % grid_height;

        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if ((i == 0 || j == 0) && (i + j != 0)) {
                    int next_index = get_index(x + i, y + j);

                    if (covered.find(next_index) == covered.end() && grid[next_index] == 0) {
                        expanded.push_back(next_index);
                        parents.push_back(search_index);
                        covered.insert(next_index);
                    }
                }
            }
        }

        search_index++;
    }

    if (expanded[search_index] == dst) {
        std::vector<int> tmp;

        while (search_index >= 0) {
            tmp.push_back(expanded[search_index]);
            search_index = parents[search_index];
        }

        // Reverse
        path.clear();
        path.reserve(tmp.size());

        for (int j = static_cast<int>(tmp.size()) - 1; j >= 0; j--)
            path.push_back(tmp[j]);
    }
}

void Room_Generator::find_best_room(std::unordered_set<int> &best_room) {
    std::unordered_set<int> all_rooms;
    all_rooms.clear();
    best_room.clear();

    int best_room_size = -1;

    int grid_size = grid_width * grid_height;

    for (int i = 0; i < grid_size; i++) {
        if (grid[i] == 0 && all_rooms.find(i) == all_rooms.end()) {
            std::unordered_set<int> next_room;
            build_room(i, next_room);
            all_rooms.insert(next_room.begin(), next_room.end());

            if (static_cast<int>(next_room.size()) > best_room_size) {
                best_room_size = next_room.size();
                best_room = next_room;
            }
        }
    }
}

void Room_Generator::expand_room(std::unordered_set<int> &set, int n) {
    std::unordered_set<int> current_set;
    current_set.insert(set.begin(), set.end());

    for (int loop = 0; loop < n; loop++) {
        std::unordered_set<int> next;

        for (int current_index : current_set) {
            if (grid[current_index] != 0)
                continue;

            int x = current_index / grid_height;
            int y = current_index % grid_height;

            for (int i = -1; i <= 1; i++) {
                for (int j = -1; j <= 1; j++) {
                    if (i != 0 || j != 0) {
                        int next_index = get_index(x + i, y + j);

                        if (set.find(next_index) == set.end() && grid[next_index] == 0) {
                            set.insert(next_index);
                            next.insert(next_index);
                        }
                    }
                }
            }
        }

        current_set = next;
    }
}
