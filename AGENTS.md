# AGENTS.md

Classic Minesweeper implemented in C++17 with Qt 6 Widgets (desktop GUI).

## Build & run

The Qt SDK is installed at `/opt/Qt/6.12.0/gcc_64` (system cmake/g++ also work).

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/Qt/6.12.0/gcc_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/minesweeper
```

- Run on a headless box: `QT_QPA_PLATFORM=offscreen ./build/minesweeper`.
- Test (no display needed): `ctest --test-dir build --output-on-failure` or `./build/test_logic`.

## Layout

- `src/minesweeper.h/.cpp` — pure game logic, zero Qt dependency (kept Qt-free so `test_logic` links without Qt). First-click-safe mine placement, BFS flood reveal, mark cycle, win/lose, undo via full-grid snapshot stack.
- `src/mainwindow.h/.cpp` — widget UI: menu (Game: New / 初·中·高级 / Ctrl+Z / Quit), mine-count + timer LCD, restart face button, `QGridLayout` of `CellButton`s. Right-click cycles 空→🚩→❓→空.
- `src/main.cpp` — entry point plus a screenshot hook.
- `tests/test_logic.cpp` — ctest target exercising first-click safety, marks, undo, win, lose.

## Quirks (surprising / non-obvious)

- **`MINESWEEPER_SHOT=<path>`** env var on the binary: saves `<path>.initial.png` and `<path>.revealed.png` (auto-right-clicks-and-reveals cell (0,0) between them) then quits. Used for headless visual verification; don't break it.
- **Revealed cells are styled via `QPalette` (Button / ButtonText roles), NOT stylesheets** — QSS `background-*` on flat `QPushButton`s renders inconsistently across windowing styles on this setup (digits took effect but backgrounds didn't). Keep it palette-based.
- **`mainwindow.h` must `#include <QPushButton>`** (no forward declaration): moc requires the full base-class definition or `moc_mainwindow.cpp` fails on the incomplete type.
- Undo restores the pre-action full-grid snapshot taken before every reveal/cycleMark; undoing the first click also removes the mines (back to `Ready`), resetting the timer.
- Test command order: `ctest` only — there is no separate lint/typecheck step for this project.