cmake_minimum_required(VERSION 3.10)
project(breakout_week4)

find_package(raylib REQUIRED)

add_executable(breakout_week4
    src/main.cpp
    src/Game.cpp
    src/Ball.cpp
    src/Paddle.cpp
    src/Brick.cpp
)

target_link_libraries(breakout_week4 raylib)
target_compile_features(breakout_week4 PRIVATE cxx_std_17)

# ✅ 强制复制 fonts 和 config.json 到 build 目录
file(COPY ${CMAKE_SOURCE_DIR}/fonts DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/src/config.json DESTINATION ${CMAKE_BINARY_DIR})