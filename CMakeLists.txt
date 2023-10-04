cmake_minimum_required(VERSION 3.18)
project(sloth3)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()


file(WRITE ${CMAKE_CURRENT_LIST_DIR}/graphics/shader_locations.h
"#include \"glad.h\"
const GLchar* SHADER_FRAGMENT = \"${CMAKE_CURRENT_LIST_DIR}/graphics/shaders/fragment.frag\";
const GLchar* SHADER_VERTEX = \"${CMAKE_CURRENT_LIST_DIR}/graphics/shaders/vertex.vert\";
"
)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -DUSE_FFTW") # USE_FFTW is used in BTrack
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(FFTW3_LIBRARY "")
set(FFTW3_INCLUDE_DIR "third_party/fftw-3.3.10")
add_subdirectory(third_party/fftw-3.3.10)

set(BUILD_TESTS OFF)
add_subdirectory(third_party/BTrack)
include_directories(third_party/BTrack/src)

set(SDL_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/third_party/SDL/include)
set(SDL_TEST OFF)
add_subdirectory(third_party/SDL)

add_library(glad third_party/glad/glad.c)
target_include_directories(glad PUBLIC third_party/glad)

add_library(imgui third_party/imgui/imgui.cpp third_party/imgui/imgui_demo.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp third_party/imgui/backends/imgui_impl_glfw.cpp third_party/imgui/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC third_party/imgui)

find_package(glfw3 3.3 REQUIRED)

add_library(fft util/fft_handler.cpp)
target_include_directories(fft PUBLIC ${CMAKE_CURRENT_LIST_DIR}/third_party/fftw-3.3.10/api)
target_link_libraries(fft PUBLIC fftw3)

add_library(util util/ring_buffer.tcc util/math.tcc util/rolling_window.tcc util/sdl_audio.tcc)
set_target_properties(util PROPERTIES LINKER_LANGUAGE CXX)

add_executable(sloth3 main.cpp)
target_include_directories(sloth3 PUBLIC third_party/linmath)
target_link_libraries(sloth3 util SDL2 fft BTrack glad glfw imgui)
