cmake_minimum_required(VERSION 3.13)
project(sat_feeder C)

set(CMAKE_C_STANDARD 99)

add_compile_options(
    -Wall -Wextra -Wshadow
    -D_POSIX_C_SOURCE=200809L
    -D_DEFAULT_SOURCE
)

# cJSON from parent project's vendored library
set(CJSON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../lib/cJSON")

add_executable(sat_feeder
    src/sat_feeder.c
    src/sat_serial.c
    src/sat_encoder.c
    ${CJSON_DIR}/cJSON.c
)

target_include_directories(sat_feeder PRIVATE
    src
    ../protocol
    ${CJSON_DIR}
)

# Strip in release mode
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET sat_feeder POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:sat_feeder>
    )
endif()
