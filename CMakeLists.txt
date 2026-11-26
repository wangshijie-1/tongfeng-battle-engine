

cmake_minimum_required(VERSION 3.10)


set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER  /usr/local/arm-linux/bin/aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER  /usr/local/arm-linux/bin/aarch64-linux-gnu-g++)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)

project(GreedySnake)


# 目录结构:
#   core/   拼装类封装(Board/Food/Snake/Screen/KeyInput/Render/rand/World_Unit)
#   modes/  模式逻辑(SingleMode)
#   根目录  引擎与入口(GameEngine/main)


aux_source_directory(. ENGINE_SRCS)      # 引擎与入口
aux_source_directory(core CORE_SRCS)     # 拼装类
aux_source_directory(modes MODE_SRCS)    # 模式逻辑


include_directories(core modes)


add_executable(GreedySnake ${ENGINE_SRCS} ${CORE_SRCS} ${MODE_SRCS})
target_link_libraries(GreedySnake pthread)

