// ============================================================================
// minesweeper.cpp — 扫雷游戏逻辑的实现 / Implementation of the Minesweeper logic.
//
// 核心思路 / Core ideas:
//   * 迟置雷：布雷推迟到"第一次左键点击"之后，从而保证首次点击安全。
//     Lazy mine placement: mines are only placed after the very first left click,
//     guaranteeing the first click is always safe.
//   * 位标志：cells_ 每个字节 8 位中，低 2 位(0/1)是地雷与翻开，bit2~3 是标记。
//     Bit flags: bits 0/1 are mine/revealed, bits 2-3 are the mark (flag/question).
//   * 快照栈撤销：任何会改变盘面的操作前，整盘拷贝压栈，撤销即整体还原。
//     Snapshot-stack undo: before every board-changing action a full copy is pushed;
//     undo simply restores the whole board.
// ============================================================================

#include "minesweeper.h"

#include <algorithm>   // std::shuffle：随机打乱布雷候选格 / shuffle used while placing mines
#include <deque>       // std::deque：洪水展开的 BFS 队列 / BFS queue for the flood reveal

// 按难度查表返回面板参数；表中没有的难度回退到初级。
// Look up the board parameters by difficulty; unknown values fall back to Beginner.
BoardConfig configFor(Difficulty d) {
    switch (d) {
        case Difficulty::Beginner:       // 初级 9×9，10 雷
            return {9, 9, 10, "初级"};
        case Difficulty::Intermediate:   // 中级 16×16，40 雷
            return {16, 16, 40, "中级"};
        case Difficulty::Expert:         // 高级 16×30，99 雷
            return {16, 30, 99, "高级"};
    }
    return {9, 9, 10, "初级"};           // 兜底返回初级 / defensive default
}

// 构造函数：直接以指定难度开一局。
// Constructor: simply starts a game at the given difficulty.
Minesweeper::Minesweeper(Difficulty d) {
    newGame(d);
}

// 开新局：重置配置、整盘清零、状态回到 Ready、清空撤销历史。
// New game: reset the config, clear the whole board, return to Ready, clear undo history.
void Minesweeper::newGame(Difficulty d) {
    cfg_ = configFor(d);                                        // 更新难度配置 / adopt the difficulty config
    cells_.assign(static_cast<size_t>(cfg_.rows) * cfg_.cols, 0);  // 所有格子清零 / zero every cell
    status_ = GameStatus::Ready;    // 未开局状态，等待首次点击 / ready and awaiting the first click
    minesPlaced_ = false;           // 尚未布雷（首次点击时才布）/ mines not placed yet
    revealed_ = 0;                  // 翻开计数清零 / reset the revealed counter
    explode_ = -1;                  // 没有爆炸雷 / no exploded mine yet
    history_.clear();               // 清空撤销栈 / drop any undo history
}

// 坐标是否在棋盘范围内 / Is (r,c) inside the board?
bool Minesweeper::inBounds(int r, int c) const {
    return r >= 0 && r < cfg_.rows && c >= 0 && c < cfg_.cols;
}

// 该格是否为地雷 / Is this cell a mine? 越界一律按非雷处理。
bool Minesweeper::isMine(int r, int c) const {
    if (!inBounds(r, c)) return false;   // 越界返回 false / out of bounds means "not a mine"
    return cells_[index(r, c)] & kMine;  // 命中地雷位 / test the mine bit
}

// 该格是否已翻开 / Has this cell been revealed?
bool Minesweeper::isRevealed(int r, int c) const {
    if (!inBounds(r, c)) return false;
    return cells_[index(r, c)] & kRevealed;  // 命中翻开位 / test the revealed bit
}

// 读取标记值：0=无标记，1=旗(🚩)，2=问号(❓)。
// Read the mark value: 0 = none, 1 = flag, 2 = question.
int Minesweeper::markValue(int r, int c) const {
    if (!inBounds(r, c)) return 0;
    return (cells_[index(r, c)] >> kMarkShift) & 0x3u;  // 取出 bit2~3 / extract mark field
}

// 统计 8 个邻居中的地雷数量（经典扫雷的提示数字）。
// Count mines among the 8 neighbours (the classic clue number).
int Minesweeper::adjacentMines(int r, int c) const {
    int count = 0;
    for (int dr = -1; dr <= 1; ++dr) {       // 行偏移 -1..1 / row offsets -1..1
        for (int dc = -1; dc <= 1; ++dc) {   // 列偏移 -1..1 / column offsets -1..1
            if (dr == 0 && dc == 0) continue;   // 跳过自身 / skip the cell itself
            if (isMine(r + dr, c + dc)) ++count;
        }
    }
    return count;
}

// 统计已插旗(标记=1)的格数，用于计算"剩余雷数"。
// Count cells marked as flags, used to compute the remaining-mine counter.
int Minesweeper::flagCount() const {
    int count = 0;
    for (size_t i = 0; i < cells_.size(); ++i) {
        uint8_t mark = (cells_[i] >> kMarkShift) & 0x3u;
        if (mark == 1) ++count;
    }
    return count;
}

// 首次点击后布雷：把雷随机布置在"首次点击格及其 3×3 邻域"以外的格子上。
// Place mines after the first click, avoiding the clicked cell and its 3×3 area.
void Minesweeper::placeMines(int safeR, int safeC) {
    std::vector<int> candidates;                 // 可布雷的候选格下标 / candidate cell indices
    candidates.reserve(cells_.size());
    for (int r = 0; r < cfg_.rows; ++r) {
        for (int c = 0; c < cfg_.cols; ++c) {
            // 位于安全区(±1)内的格子不进入候选池 / cells inside the safe zone are excluded
            bool inSafeZone = (r >= safeR - 1 && r <= safeR + 1 &&
                               c >= safeC - 1 && c <= safeC + 1);
            if (!inSafeZone) candidates.push_back(index(r, c));
        }
    }
    std::shuffle(candidates.begin(), candidates.end(), rng_);  // 打乱后再取前 N 个 / shuffle then take N
    for (int i = 0; i < cfg_.mines; ++i) {
        cells_[candidates[i]] |= kMine;   // 设置地雷位 / set the mine bit
    }
    minesPlaced_ = true;                   // 标记已布雷 / mark mines as placed
}

// BFS 洪水展开：翻开的格子若周边雷数为 0，则继续扩散到周围所有未翻开、非雷、未标记的格。
// BFS flood reveal: a revealed cell with 0 adjacent mines spreads to every
// neighbouring unrevealed, non-mine, unmarked cell.
void Minesweeper::floodReveal(int r, int c) {
    std::deque<int> queue;                   // 待处理的格下标 / cells awaiting processing
    queue.push_back(index(r, c));
    while (!queue.empty()) {
        int cur = queue.front();
        queue.pop_front();
        if (cells_[cur] & kRevealed) continue;                 // 已翻开则跳过 / skip if already revealed
        if (cells_[cur] & kMine) continue;                     // 雷不展开 / never expand onto mines
        if ((cells_[cur] >> kMarkShift) & 0x3u) continue;      // 有标记(旗/问号)的不自动展开
                                                               // skip marked cells (flag/question)
        cells_[cur] |= kRevealed;   // 翻开该格 / reveal this cell
        ++revealed_;                // 维护翻开计数 / bump the revealed counter
        int cr = cur / cfg_.cols;   // 反推行号 / recover row
        int cc = cur % cfg_.cols;   // 反推列号 / recover column
        if (adjacentMines(cr, cc) == 0) {   // 该格周边无雷才继续扩散 / expand only around empty cells
            for (int dr = -1; dr <= 1; ++dr) {
                for (int dc = -1; dc <= 1; ++dc) {
                    int nr = cr + dr;
                    int nc = cc + dc;
                    if (inBounds(nr, nc) && !(cells_[index(nr, nc)] & kRevealed)) {
                        queue.push_back(index(nr, nc));   // 入队待展开 / queue the neighbour
                    }
                }
            }
        }
    }
}

// 失败时把整盘的所有地雷都翻开（露出整片雷区）。
// When the player loses, reveal every mine so the whole minefield is exposed.
void Minesweeper::revealAllMines() {
    for (size_t i = 0; i < cells_.size(); ++i) {
        if (cells_[i] & kMine) {
            cells_[i] |= kRevealed;
        }
    }
}

// 胜利判定：翻开格数 == 总数 - 雷数 时胜利，并为所有雷自动补上旗子。
// Win check: if revealed cells == total cells - mines, the player wins and every
// remaining mine is flagged automatically.
void Minesweeper::checkWin() {
    int total = cfg_.rows * cfg_.cols;
    if (revealed_ == total - cfg_.mines) {
        status_ = GameStatus::Won;
        for (size_t i = 0; i < cells_.size(); ++i) {
            if (cells_[i] & kMine) {
                cells_[i] &= ~kMarkMask;      // 先清空原标记 / clear any previous mark
                cells_[i] |= (1u << kMarkShift);  // 置为旗子(1) / set to flag
            }
        }
    }
}

// 左键点击（翻开）/ Left-click reveal.
// 兼顾首次点击：此时尚未布雷则先布雷（并自动保证安全）。
// If mines are not yet placed (first click), place them first (guaranteed safe).
void Minesweeper::reveal(int r, int c) {
    // 胜负已定时拒绝后续操作 / ignore clicks after the game is won or lost
    if (status_ != GameStatus::Ready && status_ != GameStatus::Playing) return;
    if (!inBounds(r, c)) return;               // 越界忽略 / ignore out-of-bounds clicks
    int idx = index(r, c);
    if (cells_[idx] & kRevealed) return;       // 已翻开不再处理 / already revealed
    if ((cells_[idx] >> kMarkShift) & 0x3u) return;  // 带标记(旗/问号)的格子点不开 / marked cells are not clickable

    // 操作前压一次全盘快照，供 Ctrl+Z 撤销 / snapshot before mutating, for undo
    history_.push_back({cells_, status_, minesPlaced_, revealed_, explode_});

    if (!minesPlaced_) {             // 首次点击：此刻才布雷 / first click: place mines now
        placeMines(r, c);            // 保证以 r,c 为中心的 3×3 区域无雷 / guarantees a safe 3×3 region
        status_ = GameStatus::Playing;
    }

    if (cells_[idx] & kMine) {       // 踩到雷 → 失败 / stepping on a mine → loss
        explode_ = idx;              // 记录爆炸的那颗雷（用于红色高亮）/ record the exploded mine index
        revealAllMines();            // 掀开全部雷 / uncover every mine
        status_ = GameStatus::Lost;
        return;
    }

    floodReveal(r, c);               // 安全格：洪水展开 / safe cell: flood reveal
    checkWin();                      // 有可能因此胜利 / may have won now
}

// 右键点击：循环无→旗→问号→无 / Right-click: cycle 空→🚩→❓→空。
void Minesweeper::cycleMark(int r, int c) {
    if (status_ != GameStatus::Ready && status_ != GameStatus::Playing) return;  // 结束局禁止标记
    if (!inBounds(r, c)) return;
    int idx = index(r, c);
    if (cells_[idx] & kRevealed) return;   // 已翻开格子不能标 / revealed cells cannot be marked

    history_.push_back({cells_, status_, minesPlaced_, revealed_, explode_});  // 压快照供撤销 / snapshot for undo

    uint8_t mark = (cells_[idx] >> kMarkShift) & 0x3u;  // 读当前标记 / read the current mark
    mark = (mark + 1) % 3;                              // 前进一个状态 / advance to the next state
    cells_[idx] = (cells_[idx] & ~kMarkMask) | (mark << kMarkShift);  // 写回标记字段 / write it back
}

// 栈里是否有可撤销的操作 / Is there any undoable action on the stack?
bool Minesweeper::canUndo() const {
    return !history_.empty();
}

// 弹出并恢复最近一次快照，成功返回 true / Pop and restore the latest snapshot, true on success.
bool Minesweeper::undo() {
    if (history_.empty()) return false;      // 无历史可撤销 / nothing to undo
    Snapshot s = std::move(history_.back()); // 取出栈顶快照 / take the top snapshot
    history_.pop_back();
    cells_ = std::move(s.cells);             // 整盘状态还原 / restore the whole board
    status_ = s.status;                      // 状态、布雷标志、计数、爆炸雷一并还原
    minesPlaced_ = s.minesPlaced;
    revealed_ = s.revealed;
    explode_ = s.explode;
    return true;                             // 撤销成功 / undid successfully
}