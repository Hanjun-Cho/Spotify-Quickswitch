cmake_minimum_required(VERSION 3.16)
project(SpotifyQuickWheel CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(vendor/SDL)

add_executable(SpotifyQuickWheel 
    src/main.cpp 
    src/Input.cpp
    src/Window.cpp
    src/Wheel.cpp
    src/Http.cpp
    src/SpotifyAuth.cpp
    src/Spotify.cpp
)

target_include_directories(SpotifyQuickWheel PRIVATE include)
target_link_libraries(SpotifyQuickWheel PRIVATE SDL3::SDL3)
