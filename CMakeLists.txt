cmake_minimum_required(VERSION 3.21)
project(bezier_surfaces VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

qt_add_executable(bezier_surfaces
        src/main.cpp
        src/ui/MainWindow.cpp
        src/ui/MainWindow.h
        src/ui/Viewport.cpp
        src/ui/Viewport.h
        src/math/Vec3.h
        src/math/Mat3.h
)
target_include_directories(bezier_surfaces PRIVATE src)
target_link_libraries(bezier_surfaces PRIVATE Qt6::Widgets)

# MSVC: source files are UTF-8 (Polish characters), clang/GCC assume it by default
if(MSVC)
    target_compile_options(bezier_surfaces PRIVATE /utf-8 /W4)
else()
    target_compile_options(bezier_surfaces PRIVATE -Wall -Wextra)
endif()

set_target_properties(bezier_surfaces PROPERTIES
        WIN32_EXECUTABLE ON
        MACOSX_BUNDLE ON
)
