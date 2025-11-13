set(SERVER_TARGET anything_file_service)

# 查找SQLite3
find_library(SQLITE3_LIB sqlite3)
if(NOT SQLITE3_LIB)
    message(FATAL_ERROR "SQLite3 library not found")
endif()

# 包含当前目录
include_directories(.
    ${CMAKE_SOURCE_DIR}/3rdparty/crow/include
    ${CMAKE_SOURCE_DIR}/3rdparty/asio/include
    )

set(SERVER_SOURCES
    DBManager.cpp
    FileDB.cpp
    FileScanner.cpp
    ScanObject.cpp
    Utils.cpp
    WebService.cpp
    FileScannerManager.cpp
    main.cpp
    )

add_executable(${SERVER_TARGET} ${SERVER_SOURCES})

target_link_libraries(${SERVER_TARGET}
    /usr/lib/x86_64-linux-gnu/libsqlite3.a
    stdc++fs
    pthread
    dl
)

install(TARGETS ${SERVER_TARGET} 
    RUNTIME DESTINATION /opt/apps/com.anything/files/bin
)