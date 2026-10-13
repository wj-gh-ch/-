// ============================================================================
// mainwindow.h — 扫雷图形界面的控件定义（Qt Widgets）。
// Definition of the Minesweeper GUI widgets (Qt Widgets).
//
// 界面构成 / UI composition:
//   * CellButton —— 棋盘上的单个格子按钮（处理左键翻开、右键循环标记），
//     并对外发出 leftClicked / rightClicked 信号。
//     A single board-cell button that translates clicks into leftClicked /
//     rightClicked signals.
//   * MainWindow —— 主窗口：菜单栏 + 顶部计数/计时/表情条 + QGridLayout 棋盘。
//     The main window: menu bar + top counter/timer/face bar + QGridLayout board.
// ============================================================================
#pragma once

// 注意：这里必须写实 include 而不是前置声明（class QPushButton;）。
// NOTE: these must be real #includes, NOT forward declarations (class QPushButton;),
// because moc must see the complete base-class definition of CellButton.
#include <QGridLayout>
#include <QLCDNumber>
#include <QMainWindow>
#include <QPushButton>
#include <QTimer>
#include <QVector>

#include "minesweeper.h"   // 游戏逻辑模型 / the game-logic model

// 棋盘单元格按钮 / A single board cell button.
class CellButton : public QPushButton {
    Q_OBJECT                // 声明信号/槽必须的元对象宏 / enables signals & slots via moc
public:
    // 构造时需要记录该格的 (row, col)，以便发出带坐标的信号。
    // Construct with the cell's (row, col) so signals can carry coordinates.
    CellButton(int row, int col, QWidget* parent = nullptr);

    int row() const { return row_; }   // 行号 / row index
    int col() const { return col_; }   // 列号 / column index

signals:
    void leftClicked(int row, int col);   // 左键点击 / left-click
    void rightClicked(int row, int col);  // 右键点击 / right-click

private:
    int row_;   // 所在行 / stored row index
    int col_;   // 所在列 / stored column index
};

// 扫雷主窗口 / The main Minesweeper window.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);   // 构建菜单、头部与棋盘 / builds menus, header and board

// 槽：把控件事件连接到游戏逻辑与界面刷新。
// Slots: wire UI events to game logic and view refresh.
private slots:
    void newGame();                        // 重开当前难度的新局 / restart at the current difficulty
    void setDifficulty(Difficulty d);      // 切换难度并开新局 / switch difficulty and start fresh
    void setBeginner() { setDifficulty(Difficulty::Beginner); }       // 菜单项：初级 / menu: Beginner
    void setIntermediate() { setDifficulty(Difficulty::Intermediate); }  // 菜单项：中级 / menu: Intermediate
    void setExpert() { setDifficulty(Difficulty::Expert); }           // 菜单项：高级 / menu: Expert
    void undoMove();                       // Ctrl+Z 撤销上一步 / Ctrl+Z undo
    void onTimerTick();                    // 计时器每秒跳动 / ticked once a second by the timer
    void onCellLeft(int r, int c);         // 某格被左键点击 / a cell was left-clicked
    void onCellRight(int r, int c);        // 某格被右键点击 / a cell was right-clicked

private:
    // 界面构建与刷新辅助 / UI construction and refresh helpers.
    void buildMenu();                              // 构造"游戏"菜单 / build the Game menu
    void rebuildBoard();                           // 按当前难度重建全部的格子按钮 / rebuild all cell buttons
    void refreshAll();                             // 刷新整盘显示 / refresh the whole board view
    void refreshCell(int r, int c);                // 刷新单个格子（文本/背景/字体）/ refresh one cell's look
    void updateHeader();                           // 刷新剩余雷数 LCD / refresh the mines-left LCD
    void updateFace();                             // 刷新中间的表情按钮 / refresh the face button
    void resetTimer();                             // 停止计时并清零 / stop and zero the timer
    void stopTimerIfFinished();                    // 胜负已定时停止计时 / stop the timer when finished

    // 成员变量 / Members.
    Difficulty difficulty_{Difficulty::Beginner};  // 当前难度 / current difficulty
    Minesweeper game_;                             // 游戏逻辑模型 / the game model
    QGridLayout* grid_{nullptr};                   // 棋盘布局 / grid layout holding the cells
    QVector<CellButton*> buttons_;                 // 全部格子按钮（按下标顺序存取）/ all cell buttons
    QLCDNumber* mineLcd_{nullptr};                 // 剩余雷数显示器 / mines-left LCD
    QLCDNumber* timeLcd_{nullptr};                 // 用时显示器 / elapsed-time LCD
    QPushButton* faceButton_{nullptr};             // 重启表情按钮 🙂/😎/😵 / restart face button
    QTimer* timer_{nullptr};                       // 秒表 / 1-second timer
    int seconds_{0};                               // 已用秒数 / elapsed seconds
    bool timerStarted_{false};                     // 计时是否已经开始 / whether timing has started
};