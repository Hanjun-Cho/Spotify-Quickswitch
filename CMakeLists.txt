cmake_minimum_required(VERSION 3.16)
project(SpotifyQuickWheel C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
add_subdirectory(vendor/SDL)

# SDL3_image decodes the album art (JPEG/PNG) into an SDL_Texture.
# JPEG/PNG are handled by SDL_image's built-in stb backend, so the heavy
# format backends that pull in empty external submodules are disabled.
set(BUILD_SHARED_LIBS OFF)
set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_PNG_LIBPNG OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/SDL_image)

# SDL3_ttf renders the track name/artist text. Only the required FreeType
# backend is built (its sources are vendored); optional harfbuzz/plutosvg
# backends are disabled to avoid pulling in their external submodules.
set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/SDL_ttf)

add_executable(SpotifyQuickWheel 
    src/main.cpp 
    src/Input.cpp
    src/Window.cpp
    src/Wheel.cpp
    src/Http.cpp
    src/SpotifyAuth.cpp
    src/Spotify.cpp
    src/Tray.cpp
)

# Run as a GUI app (no console window). Combined with the /ENTRY pragma in
# main.cpp this uses int main() with the Windows subsystem.
set_target_properties(SpotifyQuickWheel PROPERTIES WIN32_EXECUTABLE ON)

target_include_directories(SpotifyQuickWheel PRIVATE include)
target_link_libraries(SpotifyQuickWheel PRIVATE SDL3::SDL3 SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf)
target_link_libraries(SpotifyQuickWheel PRIVATE shell32)

# Copy the SVG button icons next to the executable so a Release/Debug build is
# self-contained for its icons. The exe locates them via SDL_GetBasePath()
# (see Window::iconPath), so they must sit in an assets/ folder beside the exe.
add_custom_command(TARGET SpotifyQuickWheel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:SpotifyQuickWheel>/assets
    COMMENT "Copying icon assets next to SpotifyQuickWheel.exe")
