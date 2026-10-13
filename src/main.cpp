// ============================================================================
// main.cpp — 程序入口 / Program entry point.
//
// 除启动 GUI 外，还内置一个"截图钩子"：设置环境变量 MINESWEEPER_SHOT=<路径>
// 后，程序会启动窗口 → 保存 <路径>.initial.png → 模拟左键点击 (0,0) →
// 保存 <路径>.revealed.png → 自动退出。该钩子用于在无头环境下做视觉校验。
// Besides launching the GUI, an optional screenshot hook exists: with the env
// var MINESWEEPER_SHOT=<path> set, the app starts the window, saves
// <path>.initial.png, simulates a left-click on cell (0,0), saves
// <path>.revealed.png, then exits. This enables headless visual verification.
// ============================================================================

#include <QApplication>
#include <QTimer>

#include "mainwindow.h"

int main(int argc, char* argv[]) {
    // 创建 Qt 应用对象（必需，且必须在任何窗口之前）。
    // Create the Qt application object (required, and must precede any widgets).
    QApplication app(argc, argv);
    // 设置桌面应用上下文中的显示名 / set the desktop-app display names
    app.setApplicationName(QStringLiteral("minesweeper"));
    app.setApplicationDisplayName(QStringLiteral("扫雷"));

    // 构建并显示主窗口 / build and show the main window
    MainWindow window;
    window.show();

    // 截图钩子：仅在设置了 MINESWEEPER_SHOT 环境变量时生效 / active only when the env var is set.
    const QByteArray shot = qgetenv("MINESWEEPER_SHOT");
    if (!shot.isEmpty()) {
        // 延迟 400ms，确保窗口先完成显示与首帧绘制。
        // Wait 400ms so the window has been shown and painted before grabbing.
        QTimer::singleShot(400, &window, [&]() {
            // 第 1 张：初始状态 / screenshot #1: pristine board
            window.grab().save(QString::fromUtf8(shot) + QStringLiteral(".initial.png"));
            // 获取棋盘上全部格子按钮（按创建顺序无关，第一个即可），模拟一次翻开。
            // Grab any cell button and simulate a reveal of the cell.
            auto cells = window.findChildren<CellButton*>();
            if (!cells.isEmpty()) {
                cells.at(0)->click();   // 触发 CellButton::clicked → 模型 reveal / triggers a real reveal
                // 再等 400ms 让翻开结果渲染完成 / let the reveal render before the second grab
                QTimer::singleShot(400, &window, [&]() {
                    bool ok = window.grab().save(QString::fromUtf8(shot) + QStringLiteral(".revealed.png"));
                    QApplication::exit(ok ? 0 : 1);   // 保存成功才以 0 退出 / exit 0 only if saved
                });
            } else {
                QApplication::exit(0);   // 没有格子（异常）也要正常退出 / exit cleanly anyway
            }
        });
    }

    // 进入 Qt 事件循环，直到窗口关闭 / enter the Qt event loop until the window closes
    return app.exec();
}