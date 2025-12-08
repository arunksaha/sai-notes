set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(libsai SHARED
    libsai.cpp
)

target_include_directories(libsai
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
