set(CLIENT_TARGET anything)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Network)

# 自动处理MOC
set(CMAKE_AUTOMOC ON)

QT5_ADD_RESOURCES(QRC_SRCS anything.qrc)

# 源文件
set(CLIENT_SOURCES
    ScanObjDialog.cpp
    MainApp.cpp
    FileResultTable.cpp
    FileContextMenu.cpp
    HighlightDelegate.cpp
    main.cpp
    ${QRC_SRCS}
)

# 创建可执行文件
add_executable(${CLIENT_TARGET} ${CLIENT_SOURCES})

# 链接库
target_link_libraries(${CLIENT_TARGET}
    Qt5::Core 
    Qt5::Widgets 
    Qt5::Network
    stdc++fs
    pthread
    dl
)

# 安装目标
install(TARGETS ${CLIENT_TARGET} 
    RUNTIME DESTINATION /opt/apps/com.anything/files/bin
)
