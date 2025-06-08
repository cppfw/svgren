cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE $ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)

project(test)

find_package(svgren CONFIG REQUIRED)

add_executable(test main.cpp)

target_link_libraries(test PRIVATE svgren::svgren)
