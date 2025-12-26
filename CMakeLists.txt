cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME aboluo_exit)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Core project settings
project(${CMAKE_PROJECT_NAME} C CXX)
message("Build type: " ${CMAKE_BUILD_TYPE})
file(GLOB_RECURSE UI_SOURCES "ui/*.c")
file(GLOB_RECURSE LINK_SOURCES "LinkProtocol/src/*.c")
file(GLOB_RECURSE COMMANDS_SOURCES "commands/src/*.c")

# Enable CMake support for ASM and C languages
enable_language(C ASM)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)


include(FetchContent)

FetchContent_Declare(
        lvgl
        GIT_REPOSITORY https://github.com/lvgl/lvgl.git
        GIT_SHALLOW    TRUE          # 仅拉取最新层级，下载速度更快
)
FetchContent_MakeAvailable(lvgl)
# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

add_subdirectory(TF-PSA-Crypto)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${UI_SOURCES}
    ${LINK_SOURCES}
    ${COMMANDS_SOURCES}
    ${CMAKE_SOURCE_DIR}/Core/Src/atk_rgblcd.c
    ${CMAKE_SOURCE_DIR}/Core/Src/atk_rgblcd_ltdc.c
    ${CMAKE_SOURCE_DIR}/Core/Src/atk_rgblcd_touch_gtxx.c
    ${CMAKE_SOURCE_DIR}/Core/Src/atk_rgblcd_touch_iic.c
    ${CMAKE_SOURCE_DIR}/Core/Src/sdram.c
    ${CMAKE_SOURCE_DIR}/Core/Src/dwt_clk.c
    ${CMAKE_SOURCE_DIR}/lv_port_disp.c
    ${CMAKE_SOURCE_DIR}/Core/Src/U3_ASS_U1.c
    ${CMAKE_SOURCE_DIR}/Core/Src/stm32_entropy_port.c
    ${CMAKE_SOURCE_DIR}/Core/Src/crypto_utils.c
    ${CMAKE_SOURCE_DIR}/Core/Src/ble_client.c
    ${CMAKE_SOURCE_DIR}/Core/Src/app_ble.c
    ${CMAKE_SOURCE_DIR}/Core/Src/uint8_vector.c
    ${CMAKE_SOURCE_DIR}/Core/Src/auth_client.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/LinkProtocol/include
    ${lvgl_SOURCE_DIR}
    ${lvgl_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    # Add user defined include paths
)


target_link_libraries(lvgl PRIVATE stm32cubemx)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE LV_CONF_INCLUDE_SIMPLE TX_ENABLE_STACK_CHECKING
    # Add user defined symbols
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    TF-PSA-Crypto::tfpsacrypto
    lvgl
    stm32cubemx
    # Add user defined libraries
)
