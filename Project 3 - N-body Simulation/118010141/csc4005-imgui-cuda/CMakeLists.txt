cmake_minimum_required(VERSION 3.2)
project(csc4005_imgui CUDA CXX)
set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CXX_STANDARD 17)

find_package(SDL2 REQUIRED)
find_package(Freetype REQUIRED)
find_package(MPI REQUIRED)
find_package(Threads REQUIRED)
set(CMAKE_CXX_STANDARD 20)
# source scl_source enable devtoolset-10
# CC=gcc CXX=g++ cmake ..
# make -j12
# srun ./csc4005_imgui

set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -ccbin=/opt/rh/devtoolset-10/root/usr/bin/gcc -gencode=arch=compute_75,code=[sm_75,compute_75]")
set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED)

include_directories(
        include
        imgui
        imgui/backends
        ${SDL2_INCLUDE_DIRS}
        ${FREETYPE_INCLUDE_DIRS}
        ${MPI_CXX_INCLUDE_DIRS})

file(GLOB IMGUI_SRC
        imgui/*.cpp
        imgui/backends/imgui_impl_sdl.cpp
        imgui/backends/imgui_impl_opengl2.cpp
        imgui/misc/freetype/imgui_freetype.cpp
        imgui/misc/cpp/imgui_stdlib.cpp
        )

add_library(core STATIC ${IMGUI_SRC})
file(GLOB CSC4005_PROJECT_SRC src/*.cpp src/*.c src/main.cu)
add_executable(csc4005_imgui ${CSC4005_PROJECT_SRC})


set_target_properties(
        csc4005_imgui
        PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON)


get_filename_component(FONT_PATH imgui/misc/fonts/DroidSans.ttf ABSOLUTE)
target_link_libraries(core PUBLIC
        Freetype::Freetype SDL2::SDL2 OpenGL::GL ${CMAKE_DL_LIBS} Threads::Threads ${MPI_CXX_LIBRARIES})
target_link_libraries(csc4005_imgui core)
target_compile_definitions(core PUBLIC -DImDrawIdx=unsigned)
target_compile_definitions(csc4005_imgui PRIVATE -DFONT_PATH=\"${FONT_PATH}\")
if (WIN32)
    target_link_options(csc4005_imgui BEFORE PRIVATE -lmingw32 -lSDL2main -lSDL2 -mwindows)
endif()