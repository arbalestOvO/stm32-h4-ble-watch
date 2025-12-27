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

# ==============================================================================
# ESP-HOSTED MINIMAL SET (SPI + BT ONLY)
# ==============================================================================
set(EH_ROOT "Middlewares/Third_Party/esp-hosted-mcu")
set(EH_HOST_DIR "${EH_ROOT}/host")
set(EH_COMMON_DIR "${EH_ROOT}/common")

add_library(esp_hosted_lib STATIC)

# 1. 必要的驱动源文件
target_sources(esp_hosted_lib PRIVATE
        # --- 传输层核心 ---
        "${EH_HOST_DIR}/drivers/transport/transport_drv.c"
        "${EH_HOST_DIR}/drivers/transport/spi/spi_drv.c"

        # --- 串口/虚拟串口 (SPI驱动依赖) ---
        "${EH_HOST_DIR}/drivers/serial/serial_drv.c"
        "${EH_HOST_DIR}/drivers/serial/serial_ll_if.c"
        "${EH_HOST_DIR}/drivers/virtual_serial_if/serial_if.c"

        # --- 内存与工具 ---
        "${EH_HOST_DIR}/drivers/mempool/mempool.c"
        "${EH_HOST_DIR}/utils/stats.c"

        # --- 蓝牙 HCI 驱动 ---
        "${EH_HOST_DIR}/drivers/bt/hci_stub_drv.c"
        "${EH_HOST_DIR}/drivers/bt/vhci_drv.c"

        # --- STM32 移植实现 (您自己的文件) ---
        "Middlewares/Third_Party/esp-hosted-mcu/host/port/esp/threadx/src/stm32_spi_wrapper.c"
        "Middlewares/Third_Party/esp-hosted-mcu/host/port/esp/threadx/src/stm32_os_wrapper.c"

        # --- 关键 Stub (处理缺失的 RPC 依赖) ---
        "Core/Src/stm32_hosted_stub.c"  # <--- 下面会提供这个新文件
)

# 2. 头文件路径
target_include_directories(esp_hosted_lib PUBLIC
        "${EH_HOST_DIR}/api/include"
        "${EH_HOST_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/Inc"  # 确保能找到 port_config.h 和 shims
        "${EH_COMMON_DIR}"
        "${EH_COMMON_DIR}/transport"
        "${EH_COMMON_DIR}/log"
        "${EH_HOST_DIR}/port/esp/threadx/include"

)

target_include_directories(esp_hosted_lib PRIVATE
        "${EH_HOST_DIR}/drivers/transport"
        "${EH_HOST_DIR}/drivers/transport/spi"
        "${EH_HOST_DIR}/drivers/serial"
        "${EH_HOST_DIR}/drivers/virtual_serial_if"
        "${EH_HOST_DIR}/drivers/power_save"
        "${EH_HOST_DIR}/drivers/mempool"
        "${EH_HOST_DIR}/drivers/bt"
        "${EH_HOST_DIR}/utils"
        "${EH_HOST_DIR}/api/priv"
        "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/threadx/common/inc"
)

# 3. 宏定义
target_compile_definitions(esp_hosted_lib PRIVATE
        CONFIG_ESP_HOSTED_SPI_HOST_INTERFACE=1
        H_TRANSPORT_IN_USE=3

        # 定义缺失的内存宏，避免 spi_drv.c 报错
        MALLOC_CAP_DEFAULT=0
        MALLOC_CAP_8BIT=0
        MALLOC_CAP_DMA=0
        MALLOC_CAP_INTERNAL=0
)

target_link_libraries(esp_hosted_lib PRIVATE stm32cubemx)

# ==============================================================================



# =========================== NIMBLE-HOST ======================================


set(NIMBLE_ROOT "mynewt-nimble")
set(PORTING_ROOT "${NIMBLE_ROOT}/porting/npl/threadx")

# ==============================================================================
# 2. 预处理器宏定义
# ==============================================================================
add_definitions(
        -DNIMBLE_CFG_CONTROLLER=0       # 仅编译 Host (STM32)，不编译 Controller
        -DMYNEWT_VAL_BLE_HOST=1         # 启用 Host
        -DTINYCRYPT_UECC_RNG_TRNG=0     # TinyCrypt 配置 (根据是否有硬件 RNG 调整)
)

# ==============================================================================
# 3. 包含路径 (Include Directories)
# ==============================================================================
include_directories(
        # 你的配置文件 syscfg.h 所在目录
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/ST/threadx/common/inc"

        # NimBLE 核心 API
        "${NIMBLE_ROOT}/nimble/include"
        "${NIMBLE_ROOT}/nimble/host/include"
        "${NIMBLE_ROOT}/nimble/host/src"       # 部分内部头文件可能在此
        "${NIMBLE_ROOT}/nimble/host/services/gap/include"
        "${NIMBLE_ROOT}/nimble/host/services/gatt/include"
        "${NIMBLE_ROOT}/nimble/transport/include"
        "${NIMBLE_ROOT}/nimble/host/store/ram/include"
        "${NIMBLE_ROOT}/nimble/host/util/include"

        # Porting Layer 公共头文件
        "${NIMBLE_ROOT}/nimble/porting/nimble/include"
        "${NIMBLE_ROOT}/porting/nimble/include"

        # TinyCrypt (加密库)
        "${NIMBLE_ROOT}/ext/tinycrypt/include"

        # 自定义适配层头文件 (ThreadX NPL & SPI HCI)
        "${PORTING_ROOT}/include"
)

# ==============================================================================
# 4. 源文件集合 (Source Files)
# ==============================================================================
set(NIMBLE_SRCS "")

# --- A. 核心协议栈 (Host Stack) ---
file(GLOB_RECURSE NIMBLE_HOST_SRCS
        "${NIMBLE_ROOT}/nimble/host/src/*.c"
        "${NIMBLE_ROOT}/nimble/host/store/ram/src/*.c"
        "${NIMBLE_ROOT}/nimble/host/util/src/*.c"
)

list(APPEND NIMBLE_SRCS ${NIMBLE_HOST_SRCS})

# 1. Transport 核心管理 (解决 ble_transport_init 缺失)
list(APPEND NIMBLE_SRCS "${NIMBLE_ROOT}/nimble/transport/src/transport.c")

# --- B. 基础服务 (GATT Services) ---
# 至少需要 GAP 和 GATT，其他按需添加
file(GLOB NIMBLE_SVC_GAP_SRCS "${NIMBLE_ROOT}/nimble/host/services/gap/src/*.c")
file(GLOB NIMBLE_SVC_GATT_SRCS "${NIMBLE_ROOT}/nimble/host/services/gatt/src/*.c")
list(APPEND NIMBLE_SRCS ${NIMBLE_SVC_GAP_SRCS} ${NIMBLE_SVC_GATT_SRCS})

# --- C. TinyCrypt (加密依赖) ---
# NimBLE 依赖 TinyCrypt 进行配对加密
file(GLOB TINYCRYPT_SRCS
        "${NIMBLE_ROOT}/ext/tinycrypt/src/aes_encrypt.c"
        "${NIMBLE_ROOT}/ext/tinycrypt/src/utils.c"
        "${NIMBLE_ROOT}/ext/tinycrypt/src/cmac_mode.c"
        "${NIMBLE_ROOT}/ext/tinycrypt/src/ecc.c"
        "${NIMBLE_ROOT}/ext/tinycrypt/src/ecc_dh.c"
        "${NIMBLE_ROOT}/ext/tinycrypt/src/sha256.c"
)
list(APPEND NIMBLE_SRCS ${TINYCRYPT_SRCS})

# --- D. 通用移植层 (General Porting) ---
# 这些是官方提供的 OS 无关的胶水代码
list(APPEND NIMBLE_SRCS
        "${NIMBLE_ROOT}/porting/nimble/src/endian.c"
        "${NIMBLE_ROOT}/porting/nimble/src/mem.c"
        "${NIMBLE_ROOT}/porting/nimble/src/nimble_port.c" # 包含 nimble_port_init
        "${NIMBLE_ROOT}/porting/nimble/src/os_msys.c"     # Mynewt 内存缓冲管理
        "${NIMBLE_ROOT}/porting/nimble/src/os_mbuf.c"     # Mynewt 内存缓冲管理
        "${NIMBLE_ROOT}/porting/nimble/src/os_mempool.c"  # Mynewt 内存池管理
)

# --- E. [重点] 用户自定义适配层 (ThreadX & SPI) ---
# 这里引用你必须手动实现的文件
list(APPEND NIMBLE_SRCS
        "${PORTING_ROOT}/src/npl_os_threadx.c"   # ThreadX NPL 适配
        "${PORTING_ROOT}/src/ble_hci_spi.c"      # SPI HCI 传输层
)

# ==============================================================================
# 5. 生成静态库
# ==============================================================================
add_library(nimble_host STATIC ${NIMBLE_SRCS})


target_link_libraries(nimble_host PRIVATE stm32cubemx)

# ==============================================================================

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
    ${CMAKE_SOURCE_DIR}/Core/Src/test_hci.c
    ${CMAKE_SOURCE_DIR}/Core/Src/app_log.c
    ${CMAKE_SOURCE_DIR}/Core/Src/printf_redirect.c
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
    nimble_host
    esp_hosted_lib
    TF-PSA-Crypto::tfpsacrypto
    lvgl
    stm32cubemx
    # Add user defined libraries
)
