cmake_minimum_required(VERSION 3.20)

project(EpicFrameStudios VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(WIN32 AND DEFINED ENV{MSYSTEM_PREFIX})
    list(APPEND CMAKE_PREFIX_PATH "$ENV{MSYSTEM_PREFIX}")
endif()

# ---------------------------------------------------------------------------
# Dependencies fetched by the build machine (GitHub Actions). The end user's
# notebook only receives the compiled portable folder.
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.8-docking.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
set(JSON_MultipleHeaders OFF CACHE INTERNAL "")
FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

find_path(SDL2_INCLUDE_DIR NAMES SDL.h PATHS "$ENV{MSYSTEM_PREFIX}/include/SDL2" NO_DEFAULT_PATH)
if(NOT SDL2_INCLUDE_DIR)
    find_path(SDL2_INCLUDE_DIR NAMES SDL.h PATH_SUFFIXES SDL2 REQUIRED)
endif()
find_library(SDL2_LIBRARY NAMES SDL2 REQUIRED)

find_path(FFMPEG_INCLUDE_DIR NAMES libavformat/avformat.h)
find_library(AVFORMAT_LIBRARY NAMES avformat REQUIRED)
find_library(AVCODEC_LIBRARY NAMES avcodec REQUIRED)
find_library(AVUTIL_LIBRARY NAMES avutil REQUIRED)
find_library(SWSCALE_LIBRARY NAMES swscale REQUIRED)
find_library(SWRESAMPLE_LIBRARY NAMES swresample REQUIRED)

if(NOT FFMPEG_INCLUDE_DIR OR NOT SDL2_INCLUDE_DIR)
    message(FATAL_ERROR "SDL2/FFmpeg development files were not found. Run inside the MSYS2 MINGW64 shell.")
endif()

# ---------------------------------------------------------------------------
# Core library
# ---------------------------------------------------------------------------
set(EFS_CORE_SOURCES
    src/project.cpp
    src/platform.cpp
    src/media.cpp
    src/theme.cpp
    src/app.cpp
    src/app_ui.cpp
    src/inspector_ui.cpp
    src/timeline_ui.cpp
    src/selftest.cpp
)

add_library(efcore STATIC ${EFS_CORE_SOURCES})

target_include_directories(efcore PUBLIC
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${SDL2_INCLUDE_DIR}
    ${FFMPEG_INCLUDE_DIR}
)

target_sources(efcore PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)

target_compile_definitions(efcore PUBLIC
    SDL_MAIN_HANDLED
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    WINVER=0x0601
    _WIN32_WINNT=0x0601
    NTDDI_VERSION=0x06010000
    APP_VERSION="${PROJECT_VERSION}"
)

target_compile_options(efcore PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(efcore PUBLIC
    ${SDL2_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVCODEC_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${SWSCALE_LIBRARY}
    ${SWRESAMPLE_LIBRARY}
    nlohmann_json::nlohmann_json
    comdlg32 shell32 ole32 uuid winmm version
)

if(MINGW)
    target_link_options(efcore PUBLIC -static-libgcc -static-libstdc++)
endif()

add_executable(EpicFrameStudios WIN32 src/main.cpp)
target_link_libraries(EpicFrameStudios PRIVATE efcore)

add_executable(EpicFrameSelftest src/selftest_main.cpp)
target_link_libraries(EpicFrameSelftest PRIVATE efcore)

install(TARGETS EpicFrameStudios RUNTIME DESTINATION .)
