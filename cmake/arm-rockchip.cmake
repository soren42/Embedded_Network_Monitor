# CMake toolchain file for Luckfox Pico (RV1106, ARM Cortex-A7)
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/arm-rockchip.cmake ..
#
# Uses arm-linux-gnueabihf-gcc from Debian/Ubuntu (runs on aarch64 host).
# Statically links to avoid glibc/uClibc mismatch on target.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_STRIP arm-linux-gnueabihf-strip)

# ARM Cortex-A7 specific flags + static linking for uClibc target
set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -static")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static")

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
