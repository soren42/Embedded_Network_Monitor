cmake_minimum_required(VERSION 3.10)

project(embedded_netmon C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Enable POSIX APIs (timespec, sigaction, clock_gettime, etc.)
add_compile_definitions(_POSIX_C_SOURCE=200809L _DEFAULT_SOURCE)

# Compiler warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wshadow -Wundef -Wmissing-prototypes")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpointer-arith -fno-strict-aliasing -Wno-unused-parameter")
set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -DDEBUG")

# LVGL configuration: lv_conf.h is in project root
set(LV_CONF_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h
    CACHE STRING "Path to lv_conf.h")

# Add project root to includes so lv_conf.h and lv_drv_conf.h are found
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# LVGL library
add_subdirectory(lib/lvgl)

# LVGL drivers
add_subdirectory(lib/lv_drivers)

# Vendored libraries
set(LIB_SRCS
    lib/mongoose/mongoose.c
    lib/cJSON/cJSON.c
)

# Application sources
file(GLOB_RECURSE APP_SRCS
    "src/*.c"
)

add_executable(${PROJECT_NAME} ${APP_SRCS} ${LIB_SRCS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/conf
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/src/net
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/lvgl
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/lv_drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/mongoose
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/cJSON
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    lvgl
    lv_drivers
    pthread
    m
    rt
)

# Strip binary in release mode
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Stripping binary"
    )
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
