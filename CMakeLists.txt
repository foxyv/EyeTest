cmake_minimum_required(VERSION 3.20)
project(EyeTest CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(FetchContent)

# SDL2, built statically into the exe
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz)
FetchContent_MakeAvailable(SDL2)

# Dear ImGui (no CMake of its own)
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.5.tar.gz)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL2::SDL2-static)

# libusb 1.0.30 (official Windows build, DLL)
set(LIBUSB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libusb")
add_library(libusb SHARED IMPORTED)
set_target_properties(libusb PROPERTIES
    IMPORTED_LOCATION "${LIBUSB_DIR}/libusb-1.0.dll"
    IMPORTED_IMPLIB "${LIBUSB_DIR}/libusb-1.0.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBUSB_DIR}")

# PS3EYEDriver with the USB error-handling patch
file(GLOB PS3EYE_SRC third_party/ps3eye/*.cpp)
add_library(ps3eye STATIC ${PS3EYE_SRC})
target_include_directories(ps3eye PUBLIC third_party/ps3eye)
target_compile_definitions(ps3eye PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
target_link_libraries(ps3eye PUBLIC libusb)

add_executable(EyeTest WIN32 src/main.cpp src/camera_worker.cpp src/presets.cpp)
target_link_libraries(EyeTest PRIVATE ps3eye imgui SDL2::SDL2main libusb)
if(MSVC)
    target_compile_options(EyeTest PRIVATE /W3 /utf-8)
endif()

add_custom_command(TARGET EyeTest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LIBUSB_DIR}/libusb-1.0.dll" "$<TARGET_FILE_DIR:EyeTest>")
