cmake_minimum_required(VERSION 3.10)
project(breakout_week4)

find_package(raylib REQUIRED)

# ✅ 新增：添加多线程编译选项（关键！否则std::thread/std::mutex会编译失败）
add_compile_options(-pthread)

add_executable(breakout_week4
    src/main.cpp
    src/Game.cpp
    src/Ball.cpp
    src/Paddle.cpp
    src/Brick.cpp
    src/PowerUp.cpp
    src/Particle.cpp
)

# ✅ 新增：链接时添加-pthread（Linux/macOS 必需）
target_link_libraries(breakout_week4 raylib -pthread)
target_compile_features(breakout_week4 PRIVATE cxx_std_17)

# ✅ 强制复制 fonts 和 config.json 到 build 目录
file(COPY ${CMAKE_SOURCE_DIR}/fonts DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/src/config.json DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/sounds DESTINATION ${CMAKE_BINARY_DIR})