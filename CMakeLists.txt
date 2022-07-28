cmake_minimum_required(VERSION 3.16)

include(cmake/git_version.cmake)

project(TigersClav VERSION ${GIT_VERSION})

# store GIT version info to file
configure_file(src/git_version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/src/git_version.h)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS_DEBUG "-Og -g")
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(BLEND2D_STATIC TRUE)
include("blend2d/CMakeLists.txt")

find_package(FFmpeg REQUIRED COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE)
find_package(Protobuf REQUIRED)
find_package(ZLIB REQUIRED)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS
    proto/ssl_gc_referee_message.proto
    proto/ssl_gc_game_event.proto
    proto/ssl_gc_common.proto
    proto/ssl_gc_geometry.proto
    proto/ssl_vision_wrapper.proto
    proto/ssl_vision_wrapper_tracked.proto
    proto/ssl_vision_detection.proto
    proto/ssl_vision_geometry.proto
    proto/ssl_vision_detection_tracked.proto
)

include_directories(
    imgui
    imgui/backends
    ImGuiFileDialog
    src/util
    
    ${Protobuf_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_compile_options(
    -Wno-deprecated-declarations
)

add_executable(${PROJECT_NAME} 
    src/main.cpp
    src/Application.cpp
    src/TigersClav.cpp
    src/SSLGameLog.cpp
    src/LogViewer.cpp
    src/Video.cpp
    
    src/util/gzstream.cpp
    src/util/easylogging++.cc
    
    ImGuiFileDialog/ImGuiFileDialog.cpp
    src/util/CustomFont.cpp
    
    src/util/gl3w.c
    
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    
    imgui/backends/imgui_impl_opengl3.cpp
    imgui/backends/imgui_impl_glfw.cpp
    
    ${PROTO_SRCS}
)

add_definitions(
    -DCUSTOM_IMGUIFILEDIALOG_CONFIG="../src/util/CustomImGuiFileDialogConfig.h"
    -DELPP_STL_LOGGING
    -DELPP_THREAD_SAFE)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} glfw3 opengl32 Blend2D::Blend2D ${ZLIB_LIBRARIES} ${Protobuf_LIBRARIES} ${FFMPEG_LIBRARIES} -static)
    target_link_options(${PROJECT_NAME} PRIVATE -mwindows)
else()
    add_definitions(-DIMGUI_IMPL_OPENGL_ES2)
    target_link_libraries(${PROJECT_NAME} glfw GL Blend2D::Blend2D)
endif()
