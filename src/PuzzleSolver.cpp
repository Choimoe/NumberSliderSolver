// PuzzleSolver.cpp
#include "PuzzleSolver.hpp"
#include <iostream> // For debugging output (will be replaced by spdlog)
#include <thread>   // For std::thread
#include <chrono>   // For std::chrono::duration
#include <algorithm> // For std::min
#include <sstream>  // For std::ostringstream to convert thread ID to string

// TBB specific includes
#include <tbb/task_group.h>

std::vector<Solution> PuzzleSolver::solve(const Board& initial_board, SolveType type, int num_solutions_to_find, int num_threads) {
    // 初始化日志器
    if (!console_logger) {
        console_logger = spdlog::stdout_color_mt("console");
        console_logger->set_level(spdlog::level::info); // 设置日志级别，可以调成 debug
        // console_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v"); // 更详细的日志模式
    }
    console_logger->info("Starting puzzle solver with {} threads.", num_threads);
    console_logger->info("Initial Board:\n{}", initial_board.to_string());


    // 清空上次运行可能留下的数据
    // 注意：TBB 并发容器没有 clear() 方法，需要重新构造或者确保是空的
    // 这里采用重新构造的方式来确保清理
    open_set = tbb::concurrent_priority_queue<State, CompareStateForTBB>(); // 使用自定义比较器
    g_costs = tbb::concurrent_unordered_map<Board, int>();
    found_solutions.clear();
    terminate_search.store(false); // 重置终止标志
    states_explored.store(0);      // 重置探索状态计数

    // 初始化起始状态
    int initial_h = initial_board.get_manhattan_distance();
    open_set.push(State(initial_board, 0, initial_h, {initial_board})); // g_cost = 0, path 包含初始板
    g_costs.emplace(initial_board, 0); // 使用 emplace 插入

    // TBB task_group 用于管理并发任务
    tbb::task_group tg;

    // 启动多个工作线程
    for (int i = 0; i < num_threads; ++i) {
        tg.run([this, type, num_solutions_to_find] { // 使用 lambda 表达式捕获 this 和其他变量
            worker_thread_func(type, num_solutions_to_find);
        });
    }

    // 等待所有线程完成
    tg.wait();

    console_logger->info("Search finished. Total states explored: {}", states_explored.load());

    // 从 set 中提取前 num_solutions_to_find 个解决方案
    std::vector<Solution> result_solutions;
    int count = 0;
    for (const auto& sol : found_solutions) {
        if (count >= num_solutions_to_find) break;
        result_solutions.push_back(sol);
        count++;
    }

    return result_solutions;
}

void PuzzleSolver::worker_thread_func(SolveType type, int num_solutions_to_find) {
    State current_state; // 用于从 open_set 中取出的状态

    while (!terminate_search.load() && open_set.try_pop(current_state)) {
        states_explored++;

        // 获取当前最佳解决方案的成本（如果有的话），用于剪枝
        int current_nth_best_cost = -1; // 默认值表示尚未找到足够多的解
        {
            std::lock_guard<std::mutex> lock(solutions_mutex); // 保护 found_solutions
            if (found_solutions.size() >= num_solutions_to_find) {
                // 如果已找到足够数量的解，取出第 num_solutions_to_find 个解的代价
                auto it = found_solutions.begin();
                std::advance(it, num_solutions_to_find - 1);
                current_nth_best_cost = it->cost;
            }
        }

        // 如果当前状态的 f_cost 已经超过了当前第N个最佳解的成本，则可以剪枝
        // 只有当 open_set.top() (即 current_state) 的 f_cost 已经比已知最优解的成本高时，才进行剪枝
        // 这里用 f_cost 是因为 f_cost 包含了启发式，表示即使是最优情况，也无法比已找到的解更好
        if (current_nth_best_cost != -1 && current_state.f_cost >= current_nth_best_cost) {
            // 如果 open_set 的顶部元素的 f_cost 已经超过了第 N 个最佳解的成本
            // 那么后续从 open_set 中取出的解不太可能更优，可以停止当前线程的搜索
            // 但不能直接 terminate_search = true, 因为其他线程可能还有较低 f_cost 的状态
            // 应该只跳过当前状态，让其他线程继续处理。
            // 只有当 open_set 最终为空，或者所有活跃线程都无法再找到更优解时，整个搜索才会停止。
            // 更好的方式是在主循环中通过检查 open_set 和 terminate_search 的组合来控制。
            // 这里，如果当前状态自身 f_cost 较高，直接跳过处理，有助于减少不必要的扩展。
            continue;
        }


        // 如果当前状态的 g_cost 已经比已知达到该状态的最小 g_cost 大，说明找到了更优路径，跳过
        // TBB concurrent_unordered_map 的 find 和 operator[] 不同于 std::map
        // 需要使用 find + insert/update 模式
        auto g_cost_it = g_costs.find(current_state.board);
        if (g_cost_it != g_costs.end() && current_state.g_cost > g_cost_it->second) {
            continue;
        }

        // 如果达到目标状态
        if (current_state.board.is_goal()) {
            std::lock_guard<std::mutex> lock(solutions_mutex); // 保护 found_solutions
            found_solutions.insert({current_state.g_cost, current_state.path});
            
            // 使用 ostringstream 转换 thread ID 为字符串，并获取 C 字符串
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            console_logger->info("Thread {} found solution with cost: {}. Total solutions found: {}",
                                  oss.str().c_str(), // 明确传递 C 字符串
                                  current_state.g_cost, found_solutions.size());

            // 检查是否已找到足够数量的解决方案，并考虑是否可以终止所有线程
            if (found_solutions.size() >= num_solutions_to_find) {
                // 再次检查剪枝条件，如果 open_set 中最小 f_cost 已经大于第 N 个解的成本，则可以终止
                auto it = found_solutions.begin();
                std::advance(it, num_solutions_to_find - 1);
                int Nth_best_cost = it->cost;

                // TBB concurrent_priority_queue 没有 top() 方法，但 try_pop 可以用来探测顶部元素
                // 这里我们无法直接检查 open_set 的 top() 而不 pop 出来，
                // 简单的做法是，一旦找到了足够数量的解，就设置终止标志
                // 更精细的剪枝需要一个更复杂的机制，例如在所有线程都空闲时重新评估
                // 当前简单地在找到足够多解时就尝试终止，避免不必要的探索。
                terminate_search.store(true); // 设置终止标志，通知其他线程停止
            }
            continue; // 继续下一个循环，尝试弹出下一个状态
        }

        // 根据求解类型获取邻居状态
        std::vector<Board> neighbors;
        if (type == SolveType::AdjacentSwap) {
            neighbors = current_state.board.get_neighbors_adjacent_swap();
        } else { // SolveType::BlockShift
            neighbors = current_state.board.get_neighbors_block_shift();
        }

        // 遍历所有邻居
        for (const Board& neighbor_board : neighbors) {
            int new_g_cost = current_state.g_cost + 1; // 每次移动代价为 1

            // 尝试更新 g_cost。TBB concurrent_unordered_map 提供了 find 和 insert/update
            // 需要使用 `c.insert(key, value)` 和 `c.update(key, new_value)` 或 `c.emplace` 和 `c.at`
            // 这里我们使用 `tbb::concurrent_unordered_map::insert` 的重载版本，它会返回一个 pair
            // pair.first 是迭代器，pair.second 是一个 bool，表示是否插入成功 (即之前不存在)

            // 尝试插入新路径的 g_cost
            auto [it, inserted] = g_costs.emplace(neighbor_board, new_g_cost);

            if (inserted) {
                // 如果成功插入，说明是第一次访问这个邻居
                int neighbor_h = neighbor_board.get_manhattan_distance();
                std::vector<Board> new_path = current_state.path;
                new_path.push_back(neighbor_board);
                open_set.push(State(neighbor_board, new_g_cost, neighbor_h, new_path));
            } else {
                // 如果已经存在，检查是否找到了更短的路径
                if (new_g_cost < it->second) {
                    // 更新 g_cost
                    it->second = new_g_cost; // TBB concurrent_unordered_map 允许直接修改迭代器指向的值
                    int neighbor_h = neighbor_board.get_manhattan_distance();
                    std::vector<Board> new_path = current_state.path;
                    new_path.push_back(neighbor_board);
                    open_set.push(State(neighbor_board, new_g_cost, neighbor_h, new_path));
                    // 注意：这里可能会导致 open_set 中存在相同 Board 但 g_cost 较高的旧状态。
                    // A* 的标准做法是当发现更短路径时，将新状态（更低的 g_cost）重新推入优先队列。
                    // 当取出旧的、g_cost 较高的状态时，通过 `current.g_cost > g_costs[current.board]` 检查来跳过。
                    // TBB concurrent_priority_queue 不支持 "decrease-key" 操作，所以这种重复推入是常见的处理方式。
                }
            }
        }
    }
}
