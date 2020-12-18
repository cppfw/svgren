# Sets the minimum version of CMake required to build your native library.
# This ensures that a certain set of CMake features is available to
# your build.

cmake_minimum_required(VERSION 3.4.1)

# Specifies a library name, specifies whether the library is STATIC or
# SHARED, and provides relative paths to the source code. You can
# define multiple libraries by adding multiple add_library() commands,
# and CMake builds them for you. When you build your app, Gradle
# automatically packages shared libraries with your APK.

file(GLOB_RECURSE srcs "../../src/*.cpp")

include (${ANDROID_GRADLE_NATIVE_BUNDLE_PLUGIN_MK})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

add_library(
        # Specifies the name of the library.
        svgren

        # Sets the library as a shared library.
        STATIC

        # Provides a relative path to your source file(s).
        ${srcs}
    )

target_link_libraries(
        svgren
        android log ${ANDROID_GRADLE_NATIVE_MODULES}
    )
