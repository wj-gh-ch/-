// ============================================================================
// mainwindow.cpp — 扫雷图形界面的实现 / Implementation of the Minesweeper UI.
//
// 界面刷新约定 / Refresh conventions:
//   * 已翻开的格子使用 QPalette(Button/ButtonText) 着色 —— 因为在这套系统上
//     QSS 的 background-* 对扁平按钮渲染不可靠（见 AGENTS.md）。
//     Revealed cells are tinted via QPalette roles (Button/ButtonText) because
//     QSS background-* renders inconsistently on flat buttons here (see AGENTS.md).
//   * 未翻开的格子保留普通按钮外观；标记只影响文本（🚩/❓）。
//     Unrevealed cells keep the default button look; marks only change the text.
// ============================================================================

#include "mainwindow.h"

#include <QAction>
#include <QActionGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLCDNumber>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QtGlobal>
#include <array>

// 经典扫雷数字提示的配色（与 Windows 扫雷一致）。
// Classic Minesweeper digit colors, matching the original Windows game.
//   1 蓝 / blue, 2 绿 / green, 3 红 / red, 4 深蓝 / navy,
//   5 深红 / dark red, 6 青 / teal, 7 黑 / black, 8 灰 / grey.
static const QColor kNumberColors[9] = {
    QColor(0, 0, 0),        // 0: 不使用 / unused
    QColor(0, 0, 255),      // 1
    QColor(0, 128, 0),      // 2
    QColor(255, 0, 0),      // 3
    QColor(0, 0, 128),      // 4
    QColor(128, 0, 0),      // 5
    QColor(0, 128, 128),    // 6
    QColor(0, 0, 0),        // 7
    QColor(128, 128, 128),  // 8
};

// 构造一个格子按钮：记住坐标，并把"点击/右键"翻译成带坐标的信号。
// Build a cell button: store its coordinates and translate clicks into
// coordinate-carrying signals, so MainWindow need not map widgets back to cells.
CellButton::CellButton(int row, int col, QWidget* parent)
    : QPushButton(parent), row_(row), col_(col) {
    setFocusPolicy(Qt::NoFocus);                    // 避免空格/回车误触发 / keep clicks as the only input
    setContextMenuPolicy(Qt::CustomContextMenu);    // 用该策略捕获右键 / enable right-click detection
    setMinimumSize(28, 28);                         // 保证格子不会缩得太小 / a sane minimum cell size
    // 左键：由 QPushButton::clicked 转发 / left click → forward from clicked()
    connect(this, &QPushButton::clicked, this, [this]() {
        emit leftClicked(row_, col_);
    });
    // 右键：由自定义上下文菜单信号转发 / right click → forward via the custom context-menu signal
    connect(this, &QPushButton::customContextMenuRequested, this, [this](const QPoint&) {
        emit rightClicked(row_, col_);
    });
}

// 主窗口构造函数：组装菜单、顶部计数条与棋盘网格。
// MainWindow constructor: assemble the menus, the top counter bar, and the board grid.
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    buildMenu();   // 菜单栏：游戏 / menu bar: Game

    // 剩余雷数 LCD（3 位数字，扁平段式）/ mines-left LCD (3 digits, flat segments)
    mineLcd_ = new QLCDNumber(this);
    mineLcd_->setDigitCount(3);
    mineLcd_->setSegmentStyle(QLCDNumber::Flat);
    mineLcd_->setMinimumWidth(60);

    // 计时 LCD / elapsed-time LCD
    timeLcd_ = new QLCDNumber(this);
    timeLcd_->setDigitCount(3);
    timeLcd_->setSegmentStyle(QLCDNumber::Flat);
    timeLcd_->setMinimumWidth(60);

    // 中间的重启表情按钮 / the restart face button in the middle
    faceButton_ = new QPushButton(this);
    faceButton_->setFixedSize(44, 40);
    faceButton_->setFocusPolicy(Qt::NoFocus);

    // 头部布局：雷数 —— 表情 —— 计时 / header: mines-left | face | time
    auto* header = new QHBoxLayout;
    header->addWidget(mineLcd_);
    header->addStretch();
    header->addWidget(faceButton_);
    header->addStretch();
    header->addWidget(timeLcd_);

    // 棋盘网格：格子间距与边距均为 0，做出"密铺"效果。
    // Board grid: zero spacing and margins for a seamless classic look.
    grid_ = new QGridLayout;
    grid_->setSpacing(0);
    grid_->setContentsMargins(0, 0, 0, 0);

    auto* board = new QWidget(this);
    board->setLayout(grid_);

    // 中央控件：上方头部，下方棋盘（棋盘占据剩余空间）。
    // Central widget: header on top, board below (board takes the rest).
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->addLayout(header);
    layout->addWidget(board, 1);
    setCentralWidget(central);

    // 点表情按钮 = 重开新局 / clicking the face restarts the game
    connect(faceButton_, &QPushButton::clicked, this, &MainWindow::newGame);

    // 秒表：每秒触发一次 onTimerTick / 1-second timer driving onTimerTick
    timer_ = new QTimer(this);
    timer_->setInterval(1000);
    connect(timer_, &QTimer::timeout, this, &MainWindow::onTimerTick);

    // 初始界面状态 / initial UI state
    resetTimer();
    rebuildBoard();
    updateHeader();
    updateFace();
    setWindowTitle(QStringLiteral("扫雷 - %1").arg(QString::fromUtf8(game_.config().name)));
}

// 菜单："游戏" → 新游戏 / 撤销 / 难度 / 退出。
// Menu: Game → New / Undo / Difficulties / Quit.
void MainWindow::buildMenu() {
    QMenu* gameMenu = menuBar()->addMenu(QStringLiteral("游戏(&G)"));

    // 新游戏（Ctrl+N）/ New game (Ctrl+N)
    QAction* newAct = gameMenu->addAction(QStringLiteral("新游戏(&N)"));
    newAct->setShortcut(QKeySequence::New);
    connect(newAct, &QAction::triggered, this, &MainWindow::newGame);

    // 撤销（Ctrl+Z）/ Undo (Ctrl+Z)
    QAction* undoAct = gameMenu->addAction(QStringLiteral("撤销(&U)"));
    undoAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Z")));
    connect(undoAct, &QAction::triggered, this, &MainWindow::undoMove);

    gameMenu->addSeparator();

    // 难度单选组：三档互斥勾选 / an exclusive group of three difficulty choices
    auto* group = new QActionGroup(this);
    group->setExclusive(true);

    QAction* beginnerAct = gameMenu->addAction(QStringLiteral("初级 9×9，10 雷"));
    QAction* intermediateAct = gameMenu->addAction(QStringLiteral("中级 16×16，40 雷"));
    QAction* expertAct = gameMenu->addAction(QStringLiteral("高级 16×30，99 雷"));
    for (QAction* act : {beginnerAct, intermediateAct, expertAct}) {
        act->setCheckable(true);   // 可勾选 / checkable
        group->addAction(act);
    }
    beginnerAct->setChecked(true);   // 默认选中初级 / Beginner is selected by default
    connect(beginnerAct, &QAction::triggered, this, &MainWindow::setBeginner);
    connect(intermediateAct, &QAction::triggered, this, &MainWindow::setIntermediate);
    connect(expertAct, &QAction::triggered, this, &MainWindow::setExpert);

    gameMenu->addSeparator();

    // 退出（Ctrl+Q）/ Quit (Ctrl+Q)
    QAction* quitAct = gameMenu->addAction(QStringLiteral("退出(&Q)"));
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);
}

// 重开一局当前难度：清空模型、计时，重建棋盘并刷新界面。
// Restart at the current difficulty: reset the model and timer, rebuild the board, refresh UI.
void MainWindow::newGame() {
    game_.newGame(difficulty_);
    resetTimer();
    rebuildBoard();
    updateHeader();
    updateFace();
}

// 切换难度：记录难度 → 重开新局 → 更新窗口标题。
// Switch difficulty: store it, start a new game, and update the window title.
void MainWindow::setDifficulty(Difficulty d) {
    difficulty_ = d;
    newGame();
    setWindowTitle(QStringLiteral("扫雷 - %1").arg(QString::fromUtf8(game_.config().name)));
}

// 按当前棋盘尺寸重建所有格子按钮 / Rebuild every cell button for the current board size.
void MainWindow::rebuildBoard() {
    // 回收旧按钮（延迟删除，保证分发安全）/ retire old buttons via deleteLater
    for (CellButton* btn : buttons_) {
        grid_->removeWidget(btn);
        btn->deleteLater();
    }
    buttons_.clear();

    int rows = game_.rows();
    int cols = game_.cols();

    // 逐格创建按钮并接入信号 / create one button per cell and wire its signals
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            auto* btn = new CellButton(r, c);
            connect(btn, &CellButton::leftClicked, this, &MainWindow::onCellLeft);
            connect(btn, &CellButton::rightClicked, this, &MainWindow::onCellRight);
            grid_->addWidget(btn, r, c);
            buttons_.append(btn);
        }
    }
    // 行列等权拉伸，让窗口缩放时格子保持方正且自动铺满。
    // Equal row/column stretches so cells stay square and fill the window as it resizes.
    for (int r = 0; r < rows; ++r) grid_->setRowStretch(r, 1);
    for (int c = 0; c < cols; ++c) grid_->setColumnStretch(c, 1);

    refreshAll();   // 依照当前模型刷新每个格子 / render every cell from the model
}

// 刷新整盘所有格子 / Refresh the whole board.
void MainWindow::refreshAll() {
    int rows = game_.rows();
    int cols = game_.cols();
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            refreshCell(r, c);
        }
    }
}

// 刷新单个格子的外观：文本、背景色、字体 / Refresh one cell: text, background, font.
void MainWindow::refreshCell(int r, int c) {
    int idx = r * game_.cols() + c;
    CellButton* btn = buttons_[idx];

    if (game_.isRevealed(r, c)) {                  // 已掀开的格子 / revealed cells
        btn->setFlat(true);                        // 扁平化：去除凸起按钮观感 / flatten the bevel
        QPalette pal = btn->palette();

        QColor bg = QColor(0xe8, 0xe8, 0xe8);      // 默认浅灰底 / default light-grey background
        if (game_.isMine(r, c)) {                  // 雷：显示 💣，踩中的那颗用红色底高亮
            if (game_.explodedHere(r, c)) bg = QColor(0xff, 0x5a, 0x5a);
            pal.setColor(QPalette::ButtonText, QColor(0x20, 0x20, 0x20));
        } else {
            int n = game_.adjacentMines(r, c);     // 数字提示格 / numbered clue cells
            pal.setColor(QPalette::ButtonText, n > 0 ? kNumberColors[n] : bg);
        }
        pal.setColor(QPalette::Button, bg);        // 用调色板控制底/字色（见文件头说明）
        btn->setAutoFillBackground(true);          // 让背景色真正填充 / let the background colour show
        btn->setPalette(pal);

        // 字体：雷用较大表情字号；数字加粗适度字号 / fonts for mine emoji vs. digits
        QFont f = btn->font();
        f.setBold(false);
        if (game_.isMine(r, c)) {
            f.setPixelSize(16);
        } else {
            int n = game_.adjacentMines(r, c);
            if (n > 0) {
                f.setPixelSize(14);
                f.setBold(true);
            }
        }
        btn->setFont(f);
        // 文本：雷显示 💣；有提示数字显示数字；否则空白 / text: 💣 for mines, a digit, or empty
        btn->setText(game_.isMine(r, c) ? QStringLiteral("💣")
                                        : (game_.adjacentMines(r, c) > 0
                                               ? QString::number(game_.adjacentMines(r, c))
                                               : QString()));
    } else {
        // 未掀开的格子：恢复普通按钮外观 / unrevealed cells keep the normal button look
        btn->setFlat(false);
        QFont f = btn->font();
        f.setPixelSize(16);
        btn->setFont(f);
        // 只显示标记（旗 🚩 / 问号 ❓），无标记则空白 / show the mark or nothing
        int mark = game_.markValue(r, c);
        if (mark == 1) {
            btn->setText(QStringLiteral("🚩"));
        } else if (mark == 2) {
            btn->setText(QStringLiteral("❓"));
        } else {
            btn->setText(QString());
        }
    }
}

// 左键点击一个格子：转发给模型，处理计时启动/停止，刷新界面。
// A cell was left-clicked: forward to the model, handle timer start/stop, refresh UI.
void MainWindow::onCellLeft(int r, int c) {
    GameStatus before = game_.status();
    game_.reveal(r, c);
    // 首次点击使游戏进入 Playing 时，才开始记时 / start timing on the first real reveal
    if (before == GameStatus::Ready && game_.status() == GameStatus::Playing && !timerStarted_) {
        timerStarted_ = true;
        seconds_ = 0;
        timer_->start();
    }
    if (game_.won() || game_.lost()) stopTimerIfFinished();   // 终局停表 / stop the clock when finished
    refreshAll();
    updateHeader();
    updateFace();
}

// 右键点击一个格子：循环标记并刷新 / Right-click: cycle the mark and refresh.
void MainWindow::onCellRight(int r, int c) {
    game_.cycleMark(r, c);
    refreshCell(r, c);
    updateHeader();
}

// 撤销：还原模型后刷新，若回到初始态则重置计时。
// Undo: restore the model, refresh; reset the timer if the board is back to Ready.
void MainWindow::undoMove() {
    if (!game_.canUndo()) return;
    game_.undo();
    if (game_.status() == GameStatus::Ready) {
        resetTimer();                        // 撤销掉首次点击 → 未开局 / undid the first click
    } else if (game_.won() || game_.lost()) {
        stopTimerIfFinished();
    }
    refreshAll();
    updateHeader();
    updateFace();
}

// 每秒进一格计时并刷新 LCD / One tick per second, capped at 999.
void MainWindow::onTimerTick() {
    ++seconds_;
    if (seconds_ > 999) seconds_ = 999;
    timeLcd_->display(seconds_);
}

// 计时器归零：停止、清除标记、LCD 清零 / Reset the clock: stop, clear flags, zero the LCD.
void MainWindow::resetTimer() {
    timer_->stop();
    timerStarted_ = false;
    seconds_ = 0;
    timeLcd_->display(0);
}

// 终局时停止计时（胜负都停）/ Stop the clock when the game is over (win or loss).
void MainWindow::stopTimerIfFinished() {
    if (game_.won() || game_.lost()) timer_->stop();
}

// 更新剩余雷数显示：不可能为负，因此下限为 0。
// Update the mines-left display (clamped at 0 — it can never be negative).
void MainWindow::updateHeader() {
    mineLcd_->display(qMax(0, game_.remainingMines()));
}

// 更新中间表情：进行中 🙂、胜利 😎、失败 😵。
// Update the face button: 🙂 playing, 😎 won, 😵 lost.
void MainWindow::updateFace() {
    QString face;
    switch (game_.status()) {
        case GameStatus::Won:
            face = QStringLiteral("😎");
            break;
        case GameStatus::Lost:
            face = QStringLiteral("😵");
            break;
        default:
            face = QStringLiteral("🙂");
            break;
    }
    faceButton_->setText(face);
    QFont f = faceButton_->font();
    f.setPixelSize(24);   // 表情字号调大以便看清 / bigger emoji for legibility
    faceButton_->setFont(f);
}