cmake_minimum_required(VERSION 3.16)

include(cmake/git_version.cmake)

project(TigersClav VERSION ${GIT_VERSION})

# store GIT version info to file
configure_file(src/git_version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/git_version.h)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

set(BLEND2D_STATIC TRUE)
include("blend2d/CMakeLists.txt")

include_directories(
    imgui
    imgui/backends
)

add_executable(${PROJECT_NAME} 
    src/main.cpp
    src/Application.cpp
    src/TigersClav.cpp
    
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    
    imgui/backends/imgui_impl_opengl3.cpp
    imgui/backends/imgui_impl_glfw.cpp
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} glfw3 opengl32 Blend2D::Blend2D -static)
    target_link_options(${PROJECT_NAME} PRIVATE -mwindows)
else()
    add_definitions(-DIMGUI_IMPL_OPENGL_ES2)
    target_link_libraries(${PROJECT_NAME} glfw GL Blend2D::Blend2D)
endif()
