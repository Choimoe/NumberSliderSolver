# 数字华容道求解器

NumberSliderSolver 是一个基于 C++ 实现的高性能数字华容道求解器。它利用 A\* 搜索算法并结合多线程并行处理来高效地寻找给定数字华容道局面的最优解。

### 游戏目标

数字华容道游戏的目的是将一个 $N×M$ 的棋盘上的 $1$ 到 $N×M−1$ 的数字方块，通过移动空格，按照从左到右、从上到下的顺序重新排列整齐。空格通常用 $0$ 表示，并位于最终布局的右下角。

### 求解模式

本求解器支持两种不同的计分规则，并能找到每种规则下的前 5 个最优解：

1. **相邻交换计分 (Adjacent Swap)**:  
  传统的滑动拼图规则。每次将空格与相邻的数字方块进行交换，计为 $1$ 分。目标是找到总步数最少（总分最低）的解。  
2. **批量位移计分 (Sequential Block Shift)**:  
  在此模式下，一次操作可以将空格“跳过”一串连续的数字方块，使得这一整串方块同时向空格方向移动，计为 $1$ 分。例如，\[\_, 1, 2, 3\] 可以通过一次批量位移变为 \[1, 2, 3, \_\]，这仅算 $1$ 分。目标同样是找到总分最低的解。


## **构建项目**

项目使用 CMake 作为构建系统，并依赖于 spdlog 和 oneTBB 子模块。

### 1\. clone 仓库及子模块

clone 仓库并初始化子模块：

```bash
git clone https://github.com/Choimoe/NumberSliderSolver.git
cd NumberSliderSolver
git submodule update --init --recursive
```

### 2\. 构建

本项目基于 CMake，提供构建脚本，可以一键运行：

```bash
./build.sh
```

你也可以手动构建：

```bash
mkdir -p build && cd build && cmake ..
cmake --build . --config Release
```

## 运行程序

程序将从一个输入文件读取棋盘布局。默认文件名为 `puzzle_input.txt`。您也可以通过命令行参数指定输入文件和可选的时间限制（秒）。

### 1\. 创建输入文件

在项目根目录创建一个名为 `puzzle_input.txt` 的文件。

**文件格式**:

* 第一行: 两个整数 $N$ $M$ (行数和列数)，用空格分隔。  
* 接下来的 $N$ 行: 每行 $M$ 个整数，表示该行的方块值，用空格分隔。$0$ 代表空格。

**示例 `puzzle_input.txt` (3x3):**

```
3 3
2 3 6
1 5 8
4 0 7
```

**示例 `puzzle_input.txt` (4x4):**

```
4 4
13 14 12 8
0 4 10 7
6 3 9 11
1 2 15 5
```

### 2\. 执行

在 build 目录下，运行编译好的可执行文件。

```bash
# 运行默认输入文件 (puzzle_input.txt)  
./number_slider_solver

# 运行指定输入文件  
./number_slider_solver my_custom_puzzle.txt

# 运行指定输入文件并设置时间限制 (例如，10秒)  
./number_slider_solver puzzle_input.txt 10
```

程序将输出不同求解模式下的前 $5$ 个最优解的路径和总步数，以及求解所需的时间。如果设置了时间限制并在达到限制前未能找到所有解，程序将输出当前已找到的最优解。

## 许可证

本项目采用 [MIT 许可证](LICENSE)。