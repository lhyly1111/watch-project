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
set(CMAKE_PROJECT_NAME OV_Watch_APP2)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})

message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
# LVGL_ROOT 是 APP2 当前使用的 LVGL 8.2.0 源码根目录；库代码来自只读参考副本，显示端口代码会单独放入 BSP/Lvgl。
set(LVGL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/LVGL/lvgl")
# CONFIGURE_DEPENDS 让 CMake 在 LVGL 源码文件增删时自动重新生成构建规则；不包含 examples，避免把示例程序编进固件。
file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS "${LVGL_ROOT}/src/*.c")

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
        BSP/Backlight/bsp_backlight.c
        BSP/Battery/bsp_battery.c
        BSP/Key/bsp_key.c
        BSP/Lcd/bsp_lcd.c
        ${LVGL_SOURCES}
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
        BSP/Backlight
        BSP/Battery
        BSP/Key
        BSP/Lcd
        # lv_conf.h 位于该目录；LV_CONF_INCLUDE_SIMPLE 让 LVGL 从这里找到 APP2 的独立配置。
        Middlewares/LVGL
        # lvgl.h 位于该目录，APP2 之后的显示端口只需 #include "lvgl.h"。
        Middlewares/LVGL/lvgl
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
        # 告诉 LVGL 使用包含目录中的 APP2 lv_conf.h，而不是假设配置文件位于库目录上层。
        LV_CONF_INCLUDE_SIMPLE
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    # LVGL 的软件绘制可能使用数学函数；链接 Arm GNU 工具链提供的 libm。
    m

    # Add user defined libraries
)
