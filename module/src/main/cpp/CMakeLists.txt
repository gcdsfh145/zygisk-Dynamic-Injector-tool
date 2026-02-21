cmake_minimum_required(VERSION 3.22.1)
project(injector)

set(CXX_FLAGS "${CXX_FLAGS} -fno-exceptions -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_FLAGS}")

find_package(cxx REQUIRED CONFIG)
link_libraries(cxx::cxx)

# 源文件列表
set(SOURCES
    zygisk_injector.cpp
    libinjector.cpp
    injector_core.cpp
)

# 创建库
add_library(${MODULE_NAME} SHARED ${SOURCES})

# 链接系统库
target_link_libraries(${MODULE_NAME}
    log
    android
)

# 包含目录
target_include_directories(${MODULE_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 编译选项
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${MODULE_NAME} PRIVATE
        -g
        -O0
    )
endif()