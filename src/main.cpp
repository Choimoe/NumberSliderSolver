// main.cpp
#include "PuzzleSolver.hpp"
#include <iostream>
#include <vector>
#include <chrono> // 用于时间测量
#include <fstream> // 用于文件输入
#include <spdlog/spdlog.h> // spdlog 主头文件
#include <spdlog/sinks/stdout_color_sinks.h> // 用于控制台彩色输出

// 辅助函数：打印棋盘状态
void print_board(const Board& board, const std::shared_ptr<spdlog::logger>& logger) {
    std::string board_str = "";
    for (int r = 0; r < board.N; ++r) {
        for (int c = 0; c < board.M; ++c) {
            int val = board.tiles[r * board.M + c];
            board_str += (val == 0 ? "  " : (val < 10 ? " " : "")) // 格式化输出，保持对齐
                      + std::to_string(val) + " ";
        }
        board_str += "\n";
    }
    logger->info("\n{}", board_str); // 使用日志库打印
}

int main(int argc, char* argv[]) {
    // 初始化 spdlog 默认控制台日志器
    auto console_logger = spdlog::stdout_color_mt("main_logger");
    spdlog::set_default_logger(console_logger);
    spdlog::set_level(spdlog::level::info); // 设置全局日志级别为信息级
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v"); // 不显示线程ID

    // 检查命令行参数，获取输入文件名和可选的时间限制
    std::string input_filename;
    int time_limit_seconds = 0; // 默认为0，表示无时间限制

    if (argc > 1) {
        input_filename = argv[1];
        spdlog::info("Reading puzzle from file: {}", input_filename);
    } else {
        input_filename = "puzzle_input.txt"; // 默认文件名
        spdlog::warn("No input file specified. Using default: {}", input_filename);
    }

    if (argc > 2) {
        try {
            time_limit_seconds = std::stoi(argv[2]);
            if (time_limit_seconds < 0) {
                time_limit_seconds = 0; // 负数时间限制视为无限制
                spdlog::warn("Invalid time limit specified (negative). Setting to no limit.");
            }
        } catch (const std::invalid_argument& e) {
            spdlog::error("Invalid time limit argument: {}. Must be an integer. Setting to no limit.", argv[2]);
            time_limit_seconds = 0;
        } catch (const std::out_of_range& e) {
            spdlog::error("Time limit argument out of range: {}. Setting to no limit.", argv[2]);
            time_limit_seconds = 0;
        }
    }


    std::ifstream input_file(input_filename);
    if (!input_file.is_open()) {
        spdlog::error("Error: Could not open input file: {}", input_filename);
        return 1; // 退出程序
    }

    int N, M;
    if (!(input_file >> N >> M)) {
        spdlog::error("Error: Could not read N and M from input file: {}", input_filename);
        return 1;
    }
    
    if (N <= 0 || M <= 0) {
        spdlog::error("Error: Invalid board dimensions N={} M={}. N and M must be positive integers.", N, M);
        return 1;
    }

    std::vector<int> initial_tiles(N * M);
    for (int i = 0; i < N * M; ++i) {
        if (!(input_file >> initial_tiles[i])) {
            spdlog::error("Error: Could not read all tiles from input file: {}", input_filename);
            return 1;
        }
    }
    input_file.close();

    // 设置线程数量
    int num_threads = std::thread::hardware_concurrency(); // 使用所有可用的核心
    if (num_threads == 0) num_threads = 4; // 如果无法检测到核心数，默认4个线程
    spdlog::info("Detected hardware concurrency: {} threads. Using {} threads for solver.", std::thread::hardware_concurrency(), num_threads);


    // -------------------------------------------------------------
    // 求解类型 1: 相邻交换计分
    // -------------------------------------------------------------
    spdlog::info("------------------------------------------");
    spdlog::info("Solving for Type 1: Adjacent Swap ({}x{} puzzle)", N, M);
    Board initial_board_adj(N, M, initial_tiles);
    print_board(initial_board_adj, console_logger);

    PuzzleSolver solver_adj;
    auto start_time_adj = std::chrono::high_resolution_clock::now();
    // 查找前 1 个最优解，并传入时间限制
    std::vector<Solution> solutions_adj = solver_adj.solve(initial_board_adj, SolveType::AdjacentSwap, 1, num_threads, time_limit_seconds);
    auto end_time_adj = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff_adj = end_time_adj - start_time_adj;

    spdlog::info("\nSolutions (Adjacent Swap):");
    if (solutions_adj.empty()) {
        spdlog::info("No solutions found.");
    } else {
        for (size_t i = 0; i < solutions_adj.size(); ++i) {
            spdlog::info("Solution {} (Cost: {} steps):", i + 1, solutions_adj[i].cost);
            // 打印路径中的每个棋盘状态
            for (const auto& board_state : solutions_adj[i].path) {
                print_board(board_state, console_logger);
            }
            spdlog::info("--------------------");
        }
    }
    spdlog::info("Time taken for Adjacent Swap: {} seconds", diff_adj.count());


    // -------------------------------------------------------------
    // 求解类型 2: 批量位移计分
    // -------------------------------------------------------------
    spdlog::info("\n------------------------------------------");
    spdlog::info("Solving for Type 2: Sequential Block Shift ({}x{} puzzle)", N, M);
    // 重新创建初始棋盘，以确保是原始状态
    Board initial_board_block(N, M, initial_tiles);
    print_board(initial_board_block, console_logger);

    PuzzleSolver solver_block;
    auto start_time_block = std::chrono::high_resolution_clock::now();
    // 查找前 1 个最优解，并传入时间限制
    std::vector<Solution> solutions_block = solver_block.solve(initial_board_block, SolveType::BlockShift, 1, num_threads, time_limit_seconds);
    auto end_time_block = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff_block = end_time_block - start_time_block;

    spdlog::info("\nSolutions (Sequential Block Shift):");
    if (solutions_block.empty()) {
        spdlog::info("No solutions found.");
    } else {
        for (size_t i = 0; i < solutions_block.size(); ++i) {
            spdlog::info("Solution {} (Cost: {} steps):", i + 1, solutions_block[i].cost);
            // 打印路径中的每个棋盘状态
            for (const auto& board_state : solutions_block[i].path) {
                print_board(board_state, console_logger);
            }
            spdlog::info("--------------------");
        }
    }
    spdlog::info("Time taken for Sequential Block Shift: {} seconds", diff_block.count());

    return 0;
}
