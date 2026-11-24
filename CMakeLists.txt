cmake_minimum_required(VERSION 3.16)
project(skategame CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)

add_executable(skategame
    src/main.cpp
    src/world.cpp
    src/game.cpp
    src/texgen.cpp
)

target_include_directories(skategame PRIVATE ${SDL2_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS})
target_link_libraries(skategame PRIVATE ${SDL2_LIBRARIES} GLEW::GLEW OpenGL::GL)
target_compile_options(skategame PRIVATE -O2 -Wall)
