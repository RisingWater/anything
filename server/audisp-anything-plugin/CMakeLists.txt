set(TARGET audisp-anything-plugin)

find_package(Qt5 REQUIRED COMPONENTS Core Network)

set(SOURCES
    Audisp.cpp
    )

add_executable(${TARGET} ${SOURCES})

# 链接库
target_link_libraries(${TARGET}
    Qt5::Core 
    Qt5::Network
    stdc++fs
    pthread
    dl
)

install(TARGETS ${TARGET} RUNTIME DESTINATION /opt/apps/com.anything/files/bin)