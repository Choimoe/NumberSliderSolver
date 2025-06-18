// PuzzleSolver.hpp
#ifndef PUZZLE_SOLVER_HPP
#define PUZZLE_SOLVER_HPP

#include "Board.hpp"
#include <vector>
#include <string>
#include <set>        // For std::set to store unique sorted solutions
#include <atomic>     // For std::atomic_bool for termination flag
#include <mutex>      // For std::mutex for protecting shared data
#include <algorithm>  // For std::min, std::max

// TBB 并发容器
#include <tbb/concurrent_priority_queue.h>
#include <tbb/concurrent_unordered_map.h>

// spdlog 日志
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h> // For console output

// 定义求解类型
enum class SolveType {
    AdjacentSwap, // 相邻交换计分
    BlockShift    // 批量位移计分
};

// 定义 A* 算法中的状态节点
struct State {
    Board board;          // 当前棋盘状态
    int g_cost;           // 从起始状态到当前状态的实际代价（已走步数）
    int h_cost;           // 从当前状态到目标状态的启发式估计代价（曼哈顿距离）
    int f_cost;           // g_cost + h_cost (总估计代价)
    std::vector<Board> path; // 到达当前状态的路径（用于重构解）

    State() : board(0, 0, {}), g_cost(0), h_cost(0), f_cost(0) {}

    // 构造函数
    State(Board b, int g, int h, const std::vector<Board>& p) :
        board(b), g_cost(g), h_cost(h), f_cost(g + h), path(p) {}

    // 用于 std::priority_queue 的比较操作符 (用于 std::greater)
    // std::greater 使 priority_queue 成为最小堆，即 f_cost 最小的元素优先级最高
    bool operator>(const State& other) const {
        if (f_cost != other.f_cost) {
            return f_cost > other.f_cost; // 优先比较 f_cost
        }
        return g_cost > other.g_cost;     // f_cost 相同时，优先选择 g_cost 较小的（即离起始点更近的）
    }

    // 注意：这里的 operator< 已被移除，因为我们将使用自定义的 CompareStateForTBB
    // 来处理 tbb::concurrent_priority_queue 的比较逻辑。
};

// 自定义比较器，用于 tbb::concurrent_priority_queue，使其作为最小堆工作。
// 对于 max-heap， operator() 返回 true 表示第一个参数“优先级更高”（即应该在堆的顶部）。
// 在这里，f_cost 越小优先级越高，f_cost 相同则 g_cost 越小优先级越高。
struct CompareStateForTBB {
    bool operator()(const State& a, const State& b) const {
        if (a.f_cost != b.f_cost) {
            return a.f_cost < b.f_cost; // a 的 f_cost 更小，表示 a 优先级更高
        }
        return a.g_cost < b.g_cost;     // f_cost 相同，a 的 g_cost 更小，表示 a 优先级更高
    }
};

// 定义找到的解决方案结构体
struct Solution {
    int cost;             // 解决方案的总代价
    std::vector<Board> path; // 构成解决方案的棋盘状态序列

    // 用于 std::set 的比较操作符，确保解决方案按代价排序且唯一
    bool operator<(const Solution& other) const {
        if (cost != other.cost) {
            return cost < other.cost; // 优先按代价排序
        }
        // 如果代价相同，则按路径的字典序（或 Board 的字典序）排序，以确保唯一性
        // 对于相同的代价，如果路径不同，set 也能区分它们
        return path < other.path;
    }
};

// 数字华容道求解器类
class PuzzleSolver {
public:
    // 核心求解方法
    // initial_board: 初始棋盘状态
    // type: 求解类型 (相邻交换或批量位移)
    // num_solutions_to_find: 希望找到的最优解数量
    // num_threads: 线程数量
    std::vector<Solution> solve(const Board& initial_board, SolveType type, int num_solutions_to_find, int num_threads);

private:
    // 日志器
    std::shared_ptr<spdlog::logger> console_logger;

    // A* 算法所需的数据结构，现为并发版本
    // 使用自定义比较器 CompareStateForTBB
    tbb::concurrent_priority_queue<State, CompareStateForTBB> open_set;
    tbb::concurrent_unordered_map<Board, int> g_costs; // 存储到达某个棋盘状态的最小 g_cost

    // 存储找到的解决方案
    std::set<Solution> found_solutions;
    std::mutex solutions_mutex; // 保护 found_solutions

    // 终止标志
    std::atomic<bool> terminate_search; // 修正了拼写错误

    // 记录探索过的状态数量
    std::atomic<long long> states_explored;

    // 用于并行处理 A* 搜索的单个工作线程函数
    void worker_thread_func(SolveType type, int num_solutions_to_find);
};

#endif // PUZZLE_SOLVER_HPP
