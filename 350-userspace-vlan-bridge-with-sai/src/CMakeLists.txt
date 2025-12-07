set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(userspace_switch
    state/switch_state.cpp
    dataplane/switch_dataplane.cpp
    mgmtplane/switch_mgmtplane.cpp
    switch_main.cpp
)

target_include_directories(userspace_switch
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/state
        ${CMAKE_SOURCE_DIR}/libsai
)

target_link_libraries(userspace_switch PRIVATE pthread libsai)
