set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(userspace_switch
    switch_state.cpp
    switch_dataplane.cpp
    switch_main.cpp
)

target_link_libraries(userspace_switch PRIVATE pthread)
