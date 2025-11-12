cmake_minimum_required(VERSION 3.8)
project(Anything)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -Wno-unused-parameter")

add_subdirectory(server)
#add_subdirectory(client)

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