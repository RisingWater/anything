cmake_minimum_required(VERSION 3.8)
project(FileScanner)

set(TARGET anything)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

# 查找SQLite3
find_library(SQLITE3_LIB sqlite3)
if(NOT SQLITE3_LIB)
    message(FATAL_ERROR "SQLite3 library not found")
endif()

# 自动处理MOC
set(CMAKE_AUTOMOC ON)

# 包含当前目录
include_directories(.)

# 源文件
set(SOURCES
    FileDB.cpp
    ScanObject.cpp
    FileScanner.cpp
    ScanThread.cpp
    MainApp.cpp
)

# 创建可执行文件
add_executable(${TARGET} ${SOURCES})

# 链接库
target_link_libraries(${TARGET}
    Qt5::Core 
    Qt5::Widgets 
    /usr/lib/x86_64-linux-gnu/libsqlite3.a
    stdc++fs
    pthread
    dl
    )

# 安装目标（可选）
install(TARGETS ${TARGET} DESTINATION bin)