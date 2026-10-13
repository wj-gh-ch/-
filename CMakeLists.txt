# ============================================================================
# Classic Minesweeper - CMake build configuration
# 经典扫雷小游戏 —— CMake 构建配置
#
# Requires a Qt 6 SDK. Point CMake at it via CMAKE_PREFIX_PATH, e.g.:
# 需要 Qt 6 SDK，通过 CMAKE_PREFIX_PATH 指定安装路径，例如：
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/opt/Qt/6.12.0/gcc_64
# ============================================================================

# 要求 CMake 最低版本为 3.16
# Minimum supported CMake version
cmake_minimum_required(VERSION 3.16)

# 定义项目：名称、版本号、使用的语言（C++）。
# Declare the project: name, version, and language (C++).
project(minesweeper VERSION 1.0.0 LANGUAGES CXX)

# 强制使用 C++17 标准
# Enforce the C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 开启 AUTOMOC：自动为带 Q_OBJECT 的头文件调用 moc（Qt 元对象编译器），
# 用于生成信号/槽（signal/slot）所需的元对象代码。
# Enable AUTOMOC: automatically invoke moc (the Qt meta-object compiler) for
# headers containing Q_OBJECT, generating the meta-object code behind signals/slots.
set(CMAKE_AUTOMOC ON)

# 查找 Qt6 的 Widgets 模块（桌面图形界面的核心依赖）。
# Locate the Qt6 Widgets module (the core desktop-GUI dependency).
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# 主程序：入口 + 界面 + 纯游戏逻辑全部编入一个可执行文件。
# Main executable: entry point + UI + pure game logic.
add_executable(minesweeper
    src/main.cpp
    src/minesweeper.h
    src/minesweeper.cpp
    src/mainwindow.h
    src/mainwindow.cpp
)

# 链接 Qt6 Widgets 库。
# Link against the Qt6 Widgets library.
target_link_libraries(minesweeper PRIVATE Qt6::Widgets)

# 启用 CTest 测试框架。
# Enable the CTest test framework.
include(CTest)
if(BUILD_TESTING)
    # 逻辑自测程序：只编译"零 Qt 依赖"的 minesweeper.cpp，从而无需 Qt 即可运行测试。
    # Logic self-test: compiles only the Qt-free minesweeper.cpp, so the test
    # builds and runs without any Qt dependency.
    add_executable(test_logic
        tests/test_logic.cpp
        src/minesweeper.h
        src/minesweeper.cpp
    )
    # 让测试源码能找到 "minesweeper.h" 头文件。
    # Let the test source find the "minesweeper.h" header.
    target_include_directories(test_logic PRIVATE src)
    # 注册为 ctest 测试（运行 ./build/test_logic 验证纯逻辑）。
    # Register as a ctest test (runs ./build/test_logic to verify the pure logic).
    add_test(NAME logic COMMAND test_logic)
endif()