// ============================================================================
// minesweeper.h — 扫雷游戏逻辑的公共接口（纯逻辑，无任何 Qt 依赖）
// Minesweeper game-logic public interface (pure logic, no Qt dependency).
//
// 设计目标 / Design goals:
//   * 与界面层完全解耦，可在无图形环境下用 test_logic 独立测试。
//     Fully decoupled from the UI so it can be tested standalone by
//     test_logic without any display or Qt.
//   * 所有状态存放在一个 uint8_t 数组 cells_ 中，每个格子用位标志表示
//     地雷/已翻开/标记。All state lives in one uint8_t array `cells_`, each
//     cell encoding mine / revealed / mark in bit flags.
// ============================================================================
#pragma once

#include <cstdint>   // uint8_t 等定宽整数类型 / fixed-width integer types
#include <random>    // std::mt19937：布雷时用的随机数源 / RNG used for placing mines
#include <vector>    // 网格与撤销历史容器 / containers for the grid and undo history

// 三档难度 / Three difficulty levels.
// 初 级(Beginner) 9×9、10 雷；中 级(Intermediate) 16×16、40 雷；高 级(Expert) 16×30、99 雷。
enum class Difficulty { Beginner, Intermediate, Expert };

// 游戏整体状态机 / Overall game state machine.
//   Ready    —— 新局未开始，尚未布雷（等待首次点击） / game not started, mines not placed yet
//   Playing  —— 进行中，已布雷 / in progress, mines placed
//   Won      —— 已排尽所有安全格，胜利 / all safe cells revealed, victory
//   Lost     —— 触雷爆炸，失败 / hit a mine, defeat
enum class GameStatus { Ready, Playing, Won, Lost };

// 一局游戏的面板配置 / Per-game board configuration.
struct BoardConfig {
    int rows;          // 行数（高度）/ number of rows (board height)
    int cols;          // 列数（宽度）/ number of columns (board width)
    int mines;         // 地雷总数 / total mines to place
    const char* name;  // 难度显示名（如"初级"） / display name (e.g. "初级")
};

// 按难度返回对应面板配置 / Return the board config for a given difficulty.
BoardConfig configFor(Difficulty d);

// 扫雷核心逻辑类（模型层）/ The core Minesweeper model class.
class Minesweeper {
public:
    // 构造即开一局，默认初级 / Constructs and starts a game (Beginner by default).
    Minesweeper(Difficulty d = Difficulty::Beginner);

    // 以指定难度开新局，清空所有状态与撤销历史 / Start a new game of the given
    // difficulty, resetting all state and the undo history.
    void newGame(Difficulty d);

    // 只读访问器 / Read-only accessors.
    const BoardConfig& config() const { return cfg_; }  // 当前面板配置 / current board config
    GameStatus status() const { return status_; }       // 当前状态 / current status
    int rows() const { return cfg_.rows; }              // 行数 / number of rows
    int cols() const { return cfg_.cols; }              // 列数 / number of columns
    int mines() const { return cfg_.mines; }            // 地雷数 / number of mines

    // 查询单个格子：是否地雷 / 是否已翻开 / 标记值（0=无，1=旗，2=问号）+ 周边雷数。
    // Query a single cell: is it a mine / revealed / mark value (0=none,1=flag,2=question),
    // and the count of mines in the 8 neighbouring cells.
    bool isMine(int r, int c) const;
    bool isRevealed(int r, int c) const;
    int markValue(int r, int c) const;  // 0 = none, 1 = flag, 2 = question / 0=无,1=旗,2=问号
    int adjacentMines(int r, int c) const;

    // 计数与结果判断 / Counting and result checks.
    int flagCount() const;                                       // 已插旗数 / number of placed flags
    int remainingMines() const { return cfg_.mines - flagCount(); }  // 剩余待排雷数 / mines left to flag
    int revealedCount() const { return revealed_; }               // 已翻开格数 / number of revealed cells
    bool won() const { return status_ == GameStatus::Won; }       // 是否已胜 / whether the game is won
    bool lost() const { return status_ == GameStatus::Lost; }     // 是否已败 / whether the game is lost
    // 指定格子是否为"恰好踩中爆炸的那颗雷"（用于红色高亮显示）。
    // Whether this cell is the one whose mine actually exploded (shown highlighted red).
    bool explodedHere(int r, int c) const { return index(r, c) == explode_; }

    // 玩家操作 / Player actions.
    void reveal(int r, int c);      // 左键翻开 / left-click reveal
    void cycleMark(int r, int c);   // 右键循环标记：无→旗→问号→无 / right-click cycles 空→🚩→❓→空

    // 撤销 / Undo support.
    bool canUndo() const;           // 是否有可撤销的历史 / whether any history can be undone
    bool undo();                    // 撤销上一次操作并返回是否成功 / revert the last action

private:
    // 执行一次"影响网格的操作"前，对全盘状态拍一次快照，用于撤销。
    // A full snapshot of the board taken before each state-changing action, for undo.
    struct Snapshot {
        std::vector<uint8_t> cells;   // 整盘格子的位标志 / all cell bit flags
        GameStatus status;            // 当时的游戏状态 / game status at that moment
        bool minesPlaced;             // 当时是否已布雷 / whether mines had been placed
        int revealed;                 // 当时已翻开的格数 / revealed-cell count
        int explode;                  // 当时的爆炸雷下标（-1 表示无）/ exploded index (-1 = none)
    };

    // 格子位标志布局 / Per-cell bit-flag layout:
    //   bit0 = 地雷(kMine)；bit1 = 已翻开(kRevealed)；bit2~bit3 = 标记(kMarkShift)。
    //   bit0 = mine; bit1 = revealed; bits 2-3 = mark.
    static constexpr uint8_t kMine = 1 << 0;      // 地雷位 / mine bit
    static constexpr uint8_t kRevealed = 1 << 1;  // 翻开位 / revealed bit
    static constexpr uint8_t kMarkShift = 2;      // 标记起始位 / start bit of the mark field
    static constexpr uint8_t kMarkMask = 3 << kMarkShift;  // 标记字段掩码 / mask for the mark field

    // 内部工具 / Internal helpers.
    int index(int r, int c) const { return r * cfg_.cols + c; }  // 行列 → 一维下标 / (row,col) → flat index
    bool inBounds(int r, int c) const;   // 坐标是否越界 / is (r,c) inside the board
    void placeMines(int safeR, int safeC);  // 布雷并避开安全区 / place mines avoiding a safe zone
    void floodReveal(int r, int c);         // BFS 洪水式展开 / BFS flood-fill reveal
    void checkWin();                        // 检查是否胜利并自动标旗 / check win and auto-flag
    void revealAllMines();                  // 失败时掀开所有雷 / reveal every mine on a loss

    // 成员变量 / Member variables.
    BoardConfig cfg_;                          // 当前面板配置 / current board config
    std::vector<uint8_t> cells_;               // 整盘格子状态 / the whole board's cell states
    GameStatus status_{GameStatus::Ready};     // 游戏状态 / current status
    bool minesPlaced_{false};                  // 是否已布雷（首次点击后才为 true）/ mines placed yet?
    int revealed_{0};                          // 已翻开的格数 / revealed-cell counter
    int explode_{-1};                          // 当前爆炸雷下标，-1 表示无 / index of the exploded mine, -1 if none
    std::vector<Snapshot> history_;            // 撤销快照栈 / undo snapshot stack
    std::mt19937 rng_{std::random_device{}()}; // 布雷随机数发生器 / RNG for placing mines
};