include(FetchContent)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
        URL_HASH SHA256=7b42b4d6ed48810c5362c265a17faebe90dc2373c885e5216439d37927f02926
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# MSVC: use the same runtime (/MD) as the rest of the project
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(core_tests
        math/TestUtils.h
        math/Vec3Test.cpp
        math/Mat3Test.cpp
        geometry/BernsteinTest.cpp
        geometry/BezierSurfaceTest.cpp
        geometry/SurfaceParserTest.cpp
        geometry/MeshTest.cpp
        geometry/BezierCurveTest.cpp
        renderer/ProjectionTest.cpp
        renderer/MouseRotationTest.cpp
        renderer/RasterizerTest.cpp
        renderer/BarycentricTest.cpp
        renderer/ZBufferTest.cpp
        renderer/ShadingTest.cpp
        renderer/TextureTest.cpp
        renderer/NormalMappingTest.cpp
)
target_include_directories(core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(core_tests PRIVATE bezier_core GTest::gtest_main)
# Lets tests read the real data files shipped with the program
target_compile_definitions(core_tests PRIVATE BEZIER_ASSETS_DIR="${PROJECT_SOURCE_DIR}/src/assets")
bezier_set_warnings(core_tests)
# Tests don't use Qt, skip the moc/uic steps added globally by qt_standard_project_setup()
set_target_properties(core_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF)

# Registers every TEST() as a separate CTest test (also picked up by CLion's test runner)
gtest_discover_tests(core_tests DISCOVERY_MODE POST_BUILD)

# ---------- ui_tests: Qt widgets (Viewport), rendered offscreen ----------
# The ui/ sources are compiled in directly (the ui/ classes are not a separate library).
add_executable(ui_tests
        ui/main.cpp
        ui/ViewportTest.cpp
        ui/MainWindowTest.cpp
        ui/ImageLoaderTest.cpp
        ${PROJECT_SOURCE_DIR}/src/ui/Viewport.cpp
        ${PROJECT_SOURCE_DIR}/src/ui/Viewport.h
        ${PROJECT_SOURCE_DIR}/src/ui/MainWindow.cpp
        ${PROJECT_SOURCE_DIR}/src/ui/MainWindow.h
        ${PROJECT_SOURCE_DIR}/src/ui/SurfaceLoader.cpp
        ${PROJECT_SOURCE_DIR}/src/ui/SurfaceLoader.h
        ${PROJECT_SOURCE_DIR}/src/ui/ImageLoader.cpp
        ${PROJECT_SOURCE_DIR}/src/ui/ImageLoader.h
)
# MainWindow loads :/assets/surfaces.txt at startup, like the program
qt_add_resources(ui_tests "test_assets"
        PREFIX "/"
        BASE ${PROJECT_SOURCE_DIR}/src
        FILES ${PROJECT_SOURCE_DIR}/src/assets/surfaces.txt
)
target_include_directories(ui_tests PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ui_tests PRIVATE bezier_core Qt6::Widgets GTest::gtest)
target_compile_definitions(ui_tests PRIVATE BEZIER_ASSETS_DIR="${PROJECT_SOURCE_DIR}/src/assets")
bezier_set_warnings(ui_tests)

# PRE_TEST: tests are listed when ctest runs, not after the build, so a missing Qt runtime
# (e.g. DLLs not on PATH on Windows) can't break the build itself
gtest_discover_tests(ui_tests
        DISCOVERY_MODE PRE_TEST
        PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
