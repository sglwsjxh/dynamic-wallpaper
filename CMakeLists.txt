cmake_minimum_required(VERSION 3.13)

project(dynamic_wallpaper LANGUAGES CXX)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build type." FORCE)
endif()

add_executable(wallpaper WIN32
    main.cpp
    modules/logs/log.cpp
    modules/path/path.cpp
    modules/process/process.cpp
    modules/startup/startup.cpp
    modules/wallpaper/window.cpp
    modules/wallpaper/media.cpp
)

target_include_directories(wallpaper PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/modules
)

target_compile_features(wallpaper PRIVATE cxx_std_17)

set_target_properties(wallpaper PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_compile_definitions(wallpaper PRIVATE UNICODE _UNICODE)
target_compile_options(wallpaper PRIVATE -Wall)

target_include_directories(wallpaper PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv/include")
target_link_directories(wallpaper PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv/lib")

target_link_libraries(wallpaper PRIVATE
    mpv
    user32
    gdi32
    shcore
    dwmapi
    ole32
    dxgi
    "-Wl,-Bstatic"
    winpthread
    "-Wl,-Bdynamic"
)

target_link_options(wallpaper PRIVATE
    -static-libgcc
    -static-libstdc++
)

add_custom_command(TARGET wallpaper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mpv/bin/libmpv-2.dll"
            "$<TARGET_FILE_DIR:wallpaper>/libmpv-2.dll"
    VERBATIM
)