cmake_minimum_required(VERSION 3.13)

project(dynamic_wallpaper LANGUAGES CXX)

add_executable(wallpaper WIN32 main.cpp)

target_compile_features(wallpaper PRIVATE cxx_std_17)
set_target_properties(wallpaper PROPERTIES CXX_EXTENSIONS OFF)

target_compile_definitions(wallpaper PRIVATE UNICODE _UNICODE)
target_compile_options(wallpaper PRIVATE -O2 -Wall)

target_include_directories(wallpaper PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_directories(wallpaper PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# Keep the linker mode switches adjacent to winpthread so the static/dynamic
# state changes match the original g++ command exactly.
target_link_libraries(wallpaper PRIVATE
    mpv
    user32
    gdi32
    shcore
    dwmapi
    ole32
    "-Wl,-Bstatic"
    winpthread
    "-Wl,-Bdynamic"
)

target_link_options(wallpaper PRIVATE
    -static-libgcc
    -static-libstdc++
)