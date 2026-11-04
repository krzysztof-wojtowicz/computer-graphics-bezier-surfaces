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
