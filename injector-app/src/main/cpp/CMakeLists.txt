cmake_minimum_required(VERSION 3.22.1)
project("libnever")

add_library(libtool SHARED libtool.cpp)

target_include_directories(libtool PRIVATE
    ${ANDROID_NDK}/sources/cxx-stl/llvm-libc++/include
)

target_link_libraries(libtool
    log
    android
)