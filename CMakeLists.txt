cmake_minimum_required(VERSION 3.18)
project(sloth3)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra -DUSE_FFTW") # USE_FFTW is used in BTrack
set(CMAKE_CXX_FLAGS_DEBUG "-ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

add_subdirectory(third_party/fftw-3.3.10)
add_subdirectory(third_party/SDL)
set(SDL_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/third_party/SDL/include)
set(SDL_TEST OFF)

add_library(glad third_party/glad/glad.c)
target_include_directories(glad PUBLIC third_party/glad)

add_library(imgui third_party/imgui/imgui.cpp third_party/imgui/imgui_demo.cpp third_party/imgui/imgui_draw.cpp third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp third_party/imgui/backends/imgui_impl_glfw.cpp third_party/imgui/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC third_party/imgui)

find_package(glfw3 3.3 REQUIRED)

add_library(graphics graphics/realtime_audio.cpp)
target_include_directories(graphics PUBLIC third_party/linmath)
target_include_directories(graphics PUBLIC graphics)
target_link_libraries(graphics PUBLIC glad glfw imgui)

add_library(fft util/fft_handler.cpp)
target_include_directories(fft PUBLIC ${CMAKE_CURRENT_LIST_DIR}/third_party/fftw-3.3.10/api)
target_link_libraries(fft PUBLIC fftw3)

add_library(util util/ring_buffer.tcc util/math.tcc util/rolling_window.tcc)
add_library(sdl sdl/sdl_audio.tcc)
set_target_properties(util PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(graphics PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(sdl PROPERTIES LINKER_LANGUAGE CXX)

find_package(SampleRate QUIET)
add_library(btrack ${CMAKE_CURRENT_LIST_DIR}/third_party/BTrack/src/BTrack.cpp ${CMAKE_CURRENT_LIST_DIR}/third_party/BTrack/src/OnsetDetectionFunction.cpp)
target_include_directories(btrack INTERFACE $<TARGET_PROPERTY:SampleRate::samplerate,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(btrack PUBLIC ${CMAKE_CURRENT_LIST_DIR}/third_party/fftw-3.3.10/api)
target_include_directories(btrack PUBLIC ${CMAKE_CURRENT_LIST_DIR}/third_party/BTrack/src)
target_link_libraries(btrack INTERFACE SampleRate::samplerate)
target_link_libraries(btrack PUBLIC fftw3)

add_executable(sloth3 main.cpp)

target_link_libraries(sdl general SDL2)
target_link_libraries(sloth3 graphics util sdl fft btrack)
