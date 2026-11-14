cmake_minimum_required(VERSION 3.21)
project(bezier_surfaces VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

# Compiler warnings shared by all targets
# MSVC: source files are UTF-8 (Polish characters), clang/GCC assume it by default
function(bezier_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

# ---------- bezier_core: math/ and geometry/, pure C++ without Qt ----------
# Linked by the application and by the tests.
add_library(bezier_core STATIC
        src/math/Vec3.h
        src/math/Mat3.h
        src/geometry/Bernstein.h
        src/geometry/BezierSurface.h
        src/geometry/BezierSurface.cpp
        src/geometry/SurfaceParser.h
        src/geometry/SurfaceParser.cpp
        src/geometry/Mesh.h
        src/geometry/Mesh.cpp
        src/geometry/BezierCurve.h
        src/geometry/BezierCurve.cpp
        src/renderer/Projection.h
        src/renderer/MouseRotation.h
        src/renderer/Rasterizer.h
        src/renderer/Barycentric.h
        src/renderer/ZBuffer.h
        src/renderer/Shading.h
        src/renderer/Texture.h
        src/renderer/NormalMapping.h
)
target_include_directories(bezier_core PUBLIC src)
bezier_set_warnings(bezier_core)
set_target_properties(bezier_core PROPERTIES AUTOMOC OFF AUTOUIC OFF)

# ---------- bezier_surfaces: Qt application (ui/) ----------
qt_add_executable(bezier_surfaces
        src/main.cpp
        src/ui/MainWindow.cpp
        src/ui/MainWindow.h
        src/ui/Viewport.cpp
        src/ui/Viewport.h
        src/ui/SurfaceLoader.cpp
        src/ui/SurfaceLoader.h
        src/ui/ImageLoader.cpp
        src/ui/ImageLoader.h
)
target_include_directories(bezier_surfaces PRIVATE src)
target_link_libraries(bezier_surfaces PRIVATE bezier_core Qt6::Widgets)

# Data files embedded in the executable, available as ":/assets/..." (BASE strips the "src/" prefix).
# One list for the program and ui_tests; the images must match kBuiltInTextures / kBuiltInNormalMaps
# in src/ui/ImageLoader.h (checked by ImageLoaderTest).
set(BEZIER_ASSET_FILES
        src/assets/surfaces.txt
        src/assets/textures/texture_floor.png
        src/assets/textures/texture_pipes.png
        src/assets/normal_maps/normal_bricks.png
        src/assets/normal_maps/normal_circle.png
        src/assets/normal_maps/normal_floor.png
        src/assets/normal_maps/normal_hemisphere.png
        src/assets/normal_maps/normal_hex.png
        src/assets/normal_maps/normal_pipes.png
        src/assets/normal_maps/normal_rocks.png
        src/assets/normal_maps/normal_waves.png
)
qt_add_resources(bezier_surfaces "assets"
        PREFIX "/"
        BASE src
        FILES ${BEZIER_ASSET_FILES}
)
bezier_set_warnings(bezier_surfaces)

set_target_properties(bezier_surfaces PROPERTIES
        WIN32_EXECUTABLE ON
        MACOSX_BUNDLE ON
)

# ---------- Unit tests (GoogleTest, fetched at configure time) ----------
option(BEZIER_BUILD_TESTS "Build unit tests" ON)
if(BEZIER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
