cmake_minimum_required(VERSION 3.8)
project(Anything)

set(CLIENT_TARGET anything)
set(SERVER_TARGET anything_file_service)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -Wno-unused-parameter")

find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

# 查找SQLite3
find_library(SQLITE3_LIB sqlite3)
if(NOT SQLITE3_LIB)
    message(FATAL_ERROR "SQLite3 library not found")
endif()

# 自动处理MOC
set(CMAKE_AUTOMOC ON)

QT5_ADD_RESOURCES(QRC_SRCS anything.qrc)

# 包含当前目录
include_directories(.
    3rdparty/crow/include
    3rdparty/asio/include
    )

# 源文件
set(CLIENT_SOURCES
    FileDB.cpp
    ScanObject.cpp
    FileScanner.cpp
    ScanThread.cpp
    FileResultTable.cpp
    FileContextMenu.cpp
    MainApp.cpp
    ${QRC_SRCS}
)

# 创建可执行文件
add_executable(${CLIENT_TARGET} ${CLIENT_SOURCES})

set(SERVER_SOURCES
    FileDB.cpp
    FileScanner.cpp
    ScanObject.cpp
    Utils.cpp
    WebService.cpp
    FileScannerManager.cpp
    main.cpp
    )

add_executable(${SERVER_TARGET} ${SERVER_SOURCES})

# 链接库
target_link_libraries(${CLIENT_TARGET}
    Qt5::Core 
    Qt5::Widgets 
    /usr/lib/x86_64-linux-gnu/libsqlite3.a
    stdc++fs
    pthread
    dl
)

target_link_libraries(${SERVER_TARGET}
    /usr/lib/x86_64-linux-gnu/libsqlite3.a
    stdc++fs
    pthread
    dl
)

# 安装目标
install(TARGETS ${CLIENT_TARGET} 
    RUNTIME DESTINATION /opt/apps/com.anything/files/bin
)

install(TARGETS ${SERVER_TARGET} 
    RUNTIME DESTINATION /opt/apps/com.anything/files/bin
)

# 安装 desktop 文件到应用程序菜单
install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/setup/com.anything.desktop
    DESTINATION /usr/share/applications
)

# 安装 autostart desktop 文件
install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/setup/com.anything-autostart.desktop
    DESTINATION /etc/xdg/autostart
)

# 安装图标（如果有）
install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/res/anything.png
    DESTINATION /usr/share/pixmaps
)

# DEB 打包配置
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_NAME "anything")
set(CPACK_DEBIAN_PACKAGE_VERSION "1.0.0")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "RisingWater")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Fast file search utility")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION_SUMMARY "Lightning-fast desktop file search tool")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5core5a, libqt5gui5, libqt5widgets5, libc6, libstdc++6")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/RisingWater/anything")

# 包含 CPack
include(CPack)