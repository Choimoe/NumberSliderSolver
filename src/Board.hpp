#ifndef BOARD_HPP
#define BOARD_HPP

#include <vector>
#include <string>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <functional>
#include <array>

// 定义棋盘状态的结构体
struct Board {
    int N; // 行数 (rows)
    int M; // 列数 (columns)
    std::vector<int> tiles; // 棋盘上的数字，0 代表空格 (empty space)
    int empty_row;          // 空格的行坐标
    int empty_col;          // 空格的列坐标

    // 构造函数
    Board(int n, int m, const std::vector<int>& initial_tiles) : N(n), M(m), tiles(initial_tiles) {
        for (int i = 0; i < N * M; ++i) {
            if (tiles[i] == 0) {
                empty_row = i / M;
                empty_col = i % M;
                break;
            }
        }
    }

    // 拷贝构造函数和赋值运算符（默认即可，std::vector 会正确处理深拷贝）
    Board() = default;
    Board(const Board& other) = default;
    Board& operator=(const Board& other) = default;

    // 相等运算符 (用于 std::map 或 std::unordered_map 的键比较)
    bool operator==(const Board& other) const {
        return tiles == other.tiles; // 比较底层数字向量是否相同
    }

    // 小于运算符 (用于 std::map 或 std::set 的排序，或作为 std::priority_queue 的次要排序规则)
    bool operator<(const Board& other) const {
        // 字典序比较，确保 std::set<Solution> 中的 Board 路径能够被正确比较和排序
        if (N != other.N || M != other.M) {
            // 如果尺寸不同，比较尺寸，或者可以抛出异常，这里简单处理
            return N < other.N || (N == other.N && M < other.M);
        }
        return tiles < other.tiles;
    }

    // 检查当前棋盘是否达到目标状态
    // 目标状态：1, 2, ..., N*M-1, 0 (空格在最后)
    bool is_goal() const {
        for (int i = 0; i < N * M - 1; ++i) {
            if (tiles[i] != i + 1) {
                return false;
            }
        }
        return tiles[N * M - 1] == 0; // 确保最后一个位置是空格
    }

    // 计算当前棋盘的曼哈顿距离启发式值
    // 曼哈顿距离：每个数字到其目标位置的水平距离和垂直距离之和
    int get_manhattan_distance() const {
        int h = 0;
        for (int i = 0; i < N * M; ++i) {
            int val = tiles[i];
            if (val == 0) continue; // 空格不参与曼哈顿距离计算

            int current_row = i / M;
            int current_col = i % M;

            // 目标位置索引：数字 val 应该在的索引是 val - 1
            int target_idx = val - 1;
            int target_row = target_idx / M;
            int target_col = target_idx % M;

            h += std::abs(current_row - target_row) + std::abs(current_col - target_col);
        }
        return h;
    }

    // 生成相邻交换规则 (Type 1) 下的邻居状态
    std::vector<Board> get_neighbors_adjacent_swap() const {
        std::vector<Board> neighbors;
        int dr[] = {-1, 1, 0, 0}; // 方向向量：上，下，左，右
        int dc[] = {0, 0, -1, 1};

        for (int i = 0; i < 4; ++i) {
            int new_row = empty_row + dr[i];
            int new_col = empty_col + dc[i];

            // 检查新位置是否在棋盘范围内
            if (new_row >= 0 && new_row < N && new_col >= 0 && new_col < M) {
                Board new_board = *this; // 拷贝当前棋盘状态
                int new_empty_idx = new_row * M + new_col;
                int old_empty_idx = empty_row * M + empty_col;

                // 交换空格和相邻的数字
                std::swap(new_board.tiles[old_empty_idx], new_board.tiles[new_empty_idx]);
                new_board.empty_row = new_row;
                new_board.empty_col = new_col;
                neighbors.push_back(new_board);
            }
        }
        return neighbors;
    }

    // 生成批量位移规则 (Type 2) 下的邻居状态
    // 这个规则下，一次操作可以将空格“跳过”一串连续的数字方块，将整串数字批量移动，每次计1分。
    // 例如：棋盘是 [_, 1, 2, 3]。
    // 第一次循环：将1移动到空格位置 -> [1, _, 2, 3]，这作为一个新的邻居状态，成本1。
    // 第二次循环：将2移动到新的空格位置 -> [1, 2, _, 3]，这作为一个新的邻居状态，成本1。
    // 第三次循环：将3移动到新的空格位置 -> [1, 2, 3, _]，这作为一个新的邻居状态，成本1。
    // 每次“批量位移”操作（无论移动了多少个方块）都只算1分。
    std::vector<Board> get_neighbors_block_shift() const {
        std::vector<Board> neighbors;
        int dr[] = {-1, 1, 0, 0}; // 方向向量：上，下，左，右
        int dc[] = {0, 0, -1, 1};

        for (int i = 0; i < 4; ++i) { // 遍历四个方向
            int dir_r = dr[i];
            int dir_c = dc[i];

            // 用于在临时棋盘上执行批量位移
            Board current_shifted_board = *this; // 从当前棋盘开始

            // 沿着当前方向，从紧邻空格的第一个数字开始尝试批量位移
            int current_r = empty_row + dir_r;
            int current_c = empty_col + dir_c;

            while (current_r >= 0 && current_r < N && current_c >= 0 && current_c < M) {
                int current_tile_idx = current_r * M + current_c;
                int prev_empty_idx = (current_r - dir_r) * M + (current_c - dir_c);

                // 在 current_shifted_board 上执行交换，移动方块到空格原位置
                std::swap(current_shifted_board.tiles[current_tile_idx], current_shifted_board.tiles[prev_empty_idx]);

                // 更新 current_shifted_board 的空格位置
                current_shifted_board.empty_row = current_r;
                current_shifted_board.empty_col = current_c;

                // 将当前这个批量位移后的棋盘状态添加到邻居列表
                // 每次这种位移操作都算作一步
                neighbors.push_back(current_shifted_board);

                // 移动到下一个位置，考虑更大的批量位移
                current_r += dir_r;
                current_c += dir_c;
            }
        }
        return neighbors;
    }

    // 用于调试或打印棋盘状态
    std::string to_string() const {
        std::string s = "";
        for (int r = 0; r < N; ++r) {
            for (int c = 0; c < M; ++c) {
                int val = tiles[r * M + c];
                if (val == 0) {
                    s += "  "; // 空格用两个空格表示
                } else {
                    // 为了对齐，个位数前面加空格
                    s += (val < 10 ? " " : "") + std::to_string(val);
                }
                s += " ";
            }
            s += "\n";
        }
        return s;
    }
};

// 为 Board 结构体特化 std::hash，以便在 std::unordered_map 和 tbb::concurrent_unordered_map 中作为键
namespace std {
    template <>
    struct hash<Board> {
        size_t operator()(const Board& board) const {
            // 使用 Boost 的 hash_combine 思想来组合向量中元素的哈希值
            // 这是一个相对快速且分布性较好的哈希函数
            size_t seed = board.tiles.size();
            for (int i : board.tiles) {
                // 经典的 hash_combine 乘法和异或混合操作
                seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };
}

#endif // BOARD_HPP
