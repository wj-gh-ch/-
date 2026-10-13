// ============================================================================
// tests/test_logic.cpp — 纯游戏逻辑的单元自测 / Unit self-test of the game logic.
//
// 与 GUI 完全无关：只 include minesweeper.h，因此不需要 Qt 即可编译运行
// （CMake 中它也正是这样构成 test_logic 可执行文件的）。
// Fully independent of the GUI: it only includes minesweeper.h, so it builds
// and runs without Qt (which is exactly how CMake assembles the test_logic target).
//
// 覆盖点 / Coverage:
//   * 首次点击安全（3×3 无雷）、布雷确实发生   first-click safety & mines being placed
//   * 右键标记三态循环                            right-click mark cycle (空→🚩→❓→空)
//   * 撤销（含撤销首次点击、撤销标记）            undo (incl. undoing the first click and a mark)
//   * 胜利判定与自动标旗                          victory detection & auto-flagging
//   * 失败判定与爆炸格记录                        loss detection & exploded-cell tracking
// ============================================================================

#include <cstdio>          // printf：报告失败/通过信息 / for PASS/FAIL reporting
#include "minesweeper.h"   // 被测模型 / model under test

// 全局失败计数，main 里据此判定整体是否通过。
// Global failure counter; main() decides pass/fail from it.
static int failures = 0;

// 断言宏：条件不成立则累计失败并打印出错位置与表达式原文。
// Assertion macro: on failure, bump the counter and print file/line plus the
// expression text (#cond).
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            ++failures;                                                      \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
        }                                                                    \
    } while (0)

// 首次点击安全 / First-click safety: the click cell plus its 3×3 area must be mine-free.
static void testFirstClickSafety() {
    Minesweeper g(Difficulty::Beginner);
    CHECK(g.status() == GameStatus::Ready);   // 未点击前应为 Ready / Ready before any click
    g.reveal(4, 4);                           // 首次点击后自动布雷 / first click triggers mine placement
    CHECK(g.status() == GameStatus::Playing); // 应进入 Playing / should now be Playing
    // 以 (4,4) 为中心的 3×3 区域不应有雷 / the 3×3 area around (4,4) must have no mines
    for (int r = 3; r <= 5; ++r) {
        for (int c = 3; c <= 5; ++c) {
            CHECK(!g.isMine(r, c));
        }
    }
    // 整盘仍需存在雷，否则布雷失败 / mines must still exist somewhere on the board
    bool anyMine = false;
    for (int r = 0; r < g.rows(); ++r)
        for (int c = 0; c < g.cols(); ++c)
            if (g.isMine(r, c)) anyMine = true;
    CHECK(anyMine);
    CHECK(g.revealedCount() > 0);   // 至少翻开了一格 / at least the clicked cell is revealed
}

// 右键三态循环 / Right-click mark cycle: 空→旗→问号→空.
static void testMarkCycle() {
    Minesweeper g(Difficulty::Beginner);
    g.cycleMark(0, 0);                 // 空 → 🚩 / none → flag
    CHECK(g.markValue(0, 0) == 1);
    CHECK(g.flagCount() == 1);         // 旗子计数应加 1 / flag count increases by one
    CHECK(g.remainingMines() == g.mines() - 1);  // 剩余雷数相应减一 / mines-left decreases by one
    g.cycleMark(0, 0);                 // 🚩 → ❓ / flag → question
    CHECK(g.markValue(0, 0) == 2);
    CHECK(g.flagCount() == 0);         // 问号不算旗子 / a question mark is not a flag
    g.cycleMark(0, 0);                 // ❓ → 空 / question → none
    CHECK(g.markValue(0, 0) == 0);
    CHECK(g.flagCount() == 0);
}

// 撤销 / Undo: full-board restore, including un-doing a first click (mines disappear).
static void testUndo() {
    Minesweeper g(Difficulty::Beginner);
    CHECK(!g.canUndo());               // 空历史不可撤销 / nothing to undo at the start

    g.reveal(4, 4);                    // 首次点击并布雷 / first click places mines
    CHECK(g.canUndo());
    CHECK(g.revealedCount() > 0);      // 已有翻开结果 / some cells were revealed
    CHECK(g.undo());                   // 撤销后回到开局前 / undo back to the pristine state
    CHECK(g.status() == GameStatus::Ready);
    CHECK(g.revealedCount() == 0);     // 翻开全部收起 / nothing revealed any more
    CHECK(!g.canUndo());               // 历史清空 / history cleared again

    // 关键点：撤销首次点击后，雷也必须消失（尚未布雷）。
    // Key point: undoing the first click must also remove the mines (lazy placement).
    bool anyMine = false;
    for (int r = 0; r < g.rows(); ++r)
        for (int c = 0; c < g.cols(); ++c)
            if (g.isMine(r, c)) anyMine = true;
    CHECK(!anyMine);

    // 标记操作也应可撤销 / a mark operation can also be undone
    g.cycleMark(2, 2);
    CHECK(g.markValue(2, 2) == 1);
    CHECK(g.undo());
    CHECK(g.markValue(2, 2) == 0);
}

// 胜利：掀开所有非雷格后应判胜，并为全部雷自动标旗。
// Win: revealing every non-mine cell must win the game and auto-flag all mines.
static void testWin() {
    Minesweeper g(Difficulty::Intermediate);
    g.reveal(0, 0);
    CHECK(g.status() == GameStatus::Playing);
    // 只掀开非雷格，避免提前踩雷 / reveal only non-mine cells to avoid losing early
    for (int r = 0; r < g.rows(); ++r) {
        for (int c = 0; c < g.cols(); ++c) {
            if (!g.isMine(r, c)) g.reveal(r, c);
        }
    }
    CHECK(g.won());
    // 胜利时每颗雷都应自动带上旗子 / each mine should be auto-flagged on a win
    int flaggedMines = 0;
    for (int r = 0; r < g.rows(); ++r)
        for (int c = 0; c < g.cols(); ++c)
            if (g.isMine(r, c) && g.markValue(r, c) == 1) ++flaggedMines;
    CHECK(flaggedMines == g.mines());
}

// 失败：逐一翻开直到触雷，应判负并记录爆炸格，且失败后仍可撤销回 Playing。
// Loss: keep revealing until hitting a mine; the game must lose, record the
// exploded cell, and undo must bring it back to Playing.
static void testLose() {
    Minesweeper g(Difficulty::Beginner);
    int er = -1, ec = -1;
    for (int r = 0; r < g.rows() && !g.lost(); ++r) {
        for (int c = 0; c < g.cols() && !g.lost(); ++c) {
            g.reveal(r, c);
            if (g.lost()) {
                er = r;   // 记录触发失败的那颗雷坐标 / remember the exploded mine's position
                ec = c;
            }
        }
    }
    CHECK(g.lost());
    CHECK(er >= 0 && ec >= 0);                // 必须真的踩到了雷 / must have actually hit a mine
    CHECK(g.explodedHere(er, ec));            // 爆炸格应被记录 / the exploded cell is recorded
    CHECK(g.undo());                          // 失败也能撤销 / even a loss is undoable
    CHECK(g.status() == GameStatus::Playing); // 撤销后回到进行中 / restored to Playing
}

int main() {
    // 依次执行各组测试 / run the test groups in order
    testFirstClickSafety();
    testMarkCycle();
    testUndo();
    testWin();
    testLose();
    if (failures == 0) {
        std::printf("All tests passed\n");
        return 0;   // 全部通过 / all good → success
    }
    std::printf("%d check(s) failed\n", failures);
    return 1;       // 有失败 → 非零退出码 / failures → non-zero exit code
}