#include "PuzzleSolver.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <sstream>

#include <tbb/task_group.h>

std::vector<Solution> PuzzleSolver::solve(const Board& initial_board, SolveType type, int num_solutions_to_find, int num_threads, int time_limit_seconds) {
    // PuzzleSolver 将使用全局的 spdlog 默认日志器，无需在此处初始化或作为成员
    // if (!console_logger) {
    //     console_logger = spdlog::stdout_color_mt("console");
    //     console_logger->set_level(spdlog::level::info);
    // }
    spdlog::default_logger()->info("Starting puzzle solver with {} threads.", num_threads);
    if (time_limit_seconds > 0) {
        spdlog::default_logger()->info("Time limit: {} seconds.", time_limit_seconds);
    } else {
        spdlog::default_logger()->info("No time limit set.");
    }
    spdlog::default_logger()->info("Initial Board:\n{}", initial_board.to_string());


    // 清空上次运行可能留下的数据并重新构造
    open_set = tbb::concurrent_priority_queue<State, CompareStateForTBB>(); // 使用自定义比较器
    g_costs = tbb::concurrent_unordered_map<Board, int>();
    came_from = tbb::concurrent_unordered_map<Board, Board>(); // 清空 came_from map
    found_solutions.clear();
    terminate_search.store(false); // 重置终止标志
    states_explored.store(0);      // 重置探索状态计数
    // 移除了 initial_board_storage 的赋值

    // 初始化起始状态
    int initial_h = initial_board.get_manhattan_distance();
    open_set.push(State(initial_board, 0, initial_h)); // State 不再存储路径
    g_costs.emplace(initial_board, 0); // 使用 emplace 插入

    // TBB task_group 用于管理并发任务
    tbb::task_group tg;

    auto global_start_time = std::chrono::high_resolution_clock::now();

    // 启动多个工作线程
    for (int i = 0; i < num_threads; ++i) {
        // 将 initial_board, global_start_time 和 time_limit_seconds 捕获到 lambda 中，以值传递，确保线程安全
        tg.run([this, type, num_solutions_to_find, initial_board, global_start_time, time_limit_seconds] {
            worker_thread_func(type, num_solutions_to_find, initial_board, global_start_time, time_limit_seconds);
        });
    }

    // 等待所有线程完成
    tg.wait();

    if (terminate_search.load()) {
        spdlog::default_logger()->warn("Search terminated early due to time limit or solution found.");
    }
    spdlog::default_logger()->info("Search finished. Total states explored: {}", states_explored.load());

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

void PuzzleSolver::worker_thread_func(SolveType type, int num_solutions_to_find, const Board& initial_board_for_reconstruction,
                                     std::chrono::high_resolution_clock::time_point start_time, int time_limit_seconds) {
    State current_state; // 用于从 open_set 中取出的状态
    auto last_log_time = std::chrono::high_resolution_clock::now();

    while (!terminate_search.load() && open_set.try_pop(current_state)) {
        states_explored++;

        // 周期性日志，监控搜索进展
        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time).count() >= 5) { // 每5秒记录一次
            std::ostringstream oss_id;
            oss_id << std::this_thread::get_id();
            spdlog::default_logger()->info("Thread {}: Explored {} states. Open set size: {}. G_costs size: {}",
                                  oss_id.str().c_str(), states_explored.load(), open_set.size(), g_costs.size());
            last_log_time = now;
        }

        // 检查是否超时
        if (time_limit_seconds > 0 && std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count() >= time_limit_seconds) {
            std::ostringstream oss_id; // 局部 stringstream for this log
            oss_id << std::this_thread::get_id();
            spdlog::default_logger()->warn("Thread {} reached time limit of {} seconds. Terminating search.",
                                           oss_id.str().c_str(), time_limit_seconds); // 明确传递 C 字符串
            terminate_search.store(true); // 设置终止标志，通知其他线程停止
            break; // 退出当前线程的循环
        }

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
        if (current_nth_best_cost != -1 && current_state.f_cost >= current_nth_best_cost) {
            continue; // 跳过此状态，继续从 open_set 取出下一个
        }

        // 如果当前状态的 g_cost 已经比已知达到该状态的最小 g_cost 大，说明找到了更优路径，跳过
        auto g_cost_it = g_costs.find(current_state.board);
        if (g_cost_it != g_costs.end() && current_state.g_cost > g_cost_it->second) {
            continue;
        }

        // 如果达到目标状态
        if (current_state.board.is_goal()) {
            std::lock_guard<std::mutex> lock(solutions_mutex); // 保护 found_solutions
            // 重建路径，将初始棋盘传递给 reconstruct_path
            std::vector<Board> path = reconstruct_path(current_state.board, initial_board_for_reconstruction);
            found_solutions.insert({current_state.g_cost, path});
            
            // 使用 ostringstream 转换 thread ID 为字符串，并获取 C 字符串
            std::ostringstream oss;
            oss << std::this_thread::get_id();
            spdlog::default_logger()->info("Thread {} found solution with cost: {}. Total solutions found: {}",
                                  oss.str().c_str(), // 明确传递 C 字符串
                                  current_state.g_cost, found_solutions.size());

            // 检查是否已找到足够数量的解决方案，并考虑是否可以终止所有线程
            if (found_solutions.size() >= num_solutions_to_find) {
                // 如果已找到指定数量的最优解，可以考虑终止所有线程
                // 这里我们保守地设置终止标志，但线程会继续处理队列中已存在的较低 f_cost 状态
                terminate_search.store(true);
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

            // 尝试插入或更新 g_cost 和 came_from 映射
            // 注意：tbb::concurrent_unordered_map 的 emplace/insert/update 机制
            // 这里我们希望在找到更短路径时，更新 came_from 并重新加入 open_set
            
            // 尝试插入新的 g_cost
            auto [it_g, inserted_g] = g_costs.emplace(neighbor_board, new_g_cost);

            if (inserted_g) {
                // 如果成功插入，说明是第一次访问这个邻居
                int neighbor_h = neighbor_board.get_manhattan_distance();
                open_set.push(State(neighbor_board, new_g_cost, neighbor_h));
                came_from.emplace(neighbor_board, current_state.board); // 记录父子关系
            } else {
                // 如果 g_cost 已经存在，检查是否找到了更短的路径
                if (new_g_cost < it_g->second) {
                    // 更新 g_cost
                    it_g->second = new_g_cost; // 更新已存在的 g_cost
                    int neighbor_h = neighbor_board.get_manhattan_distance();
                    open_set.push(State(neighbor_board, new_g_cost, neighbor_h)); // 将更新后的状态重新推入优先队列

                    // 更新 came_from。由于 neighbor_board 在此分支中必然已存在于 came_from (因为它存在于 g_costs)，
                    // 可以安全地使用 operator[] 来更新其关联的值。
                    came_from[neighbor_board] = current_state.board;
                }
            }
        }
    }
}

// 辅助函数：从 came_from 映射重建路径
// 现在接收 initial_board 作为参数
std::vector<Board> PuzzleSolver::reconstruct_path(const Board& goal_board, const Board& initial_board) {
    std::vector<Board> path;
    Board current = goal_board;

    // 回溯直到找到初始棋盘
    while (!(current == initial_board)) { // 使用传入的 initial_board 作为终止条件
        path.push_back(current);
        // 在 came_from 中查找当前棋盘的父棋盘
        auto it = came_from.find(current);
        if (it != came_from.end()) {
            current = it->second; // 移动到父棋盘
        } else {
            // 这不应该发生，除非路径跟踪逻辑有问题或初始状态未正确识别
            spdlog::default_logger()->error("Error: Could not reconstruct path for board: \n{}", current.to_string());
            break; // 避免无限循环
        }
    }
    path.push_back(initial_board); // 添加初始棋盘

    std::reverse(path.begin(), path.end()); // 路径是逆序的，需要反转
    return path;
}
