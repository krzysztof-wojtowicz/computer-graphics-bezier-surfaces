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

add_executable(math_tests
        math/TestUtils.h
        math/Vec3Test.cpp
        math/Mat3Test.cpp
)
target_include_directories(math_tests PRIVATE ${PROJECT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(math_tests PRIVATE GTest::gtest_main)

if(MSVC)
    target_compile_options(math_tests PRIVATE /utf-8 /W4)
else()
    target_compile_options(math_tests PRIVATE -Wall -Wextra)
endif()

# Registers every TEST() as a separate CTest test (also picked up by CLion's test runner)
gtest_discover_tests(math_tests DISCOVERY_MODE POST_BUILD)
