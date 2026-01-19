# PICO SDK defaults to 3.13 minimum
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

# Yarg device selection: RASPBERRY_PI_PICO or GENERIC_HOST
# determines if cyarg is built as hosted or self-hosted binary
# for Pico, sets the PICO_BOARD variable for the Pico SDK
if (YARG_DEVICE STREQUAL "RASPBERRY_PI_PICO")
set(CYARG_HOSTING "SELF_HOSTED" CACHE STRING "cyarg built as self-hosting binary")
set(PICO_BOARD "pico" CACHE STRING "Raspberry Pi Pico board")
elseif (YARG_DEVICE STREQUAL "GENERIC_HOST")
set(CYARG_HOSTING "HOSTED" CACHE STRING "cyarg built as hosted binary")
endif()

# configures defaults for hosting and self hosted builds.
if (CYARG_HOSTING STREQUAL "SELF_HOSTED")
add_compile_definitions(CYARG_SELF_HOSTED)
set(CYARG_FEATURE_FILESYSTEM "LITTLEFS" CACHE STRING "Filesystem feature to include")
elseif(CYARG_HOSTING STREQUAL "HOSTED")
add_compile_definitions(CYARG_OS_HOSTED)
set(CYARG_FEATURE_FILESYSTEM "CSTDLIB" CACHE STRING "Filesystem feature to include")
set(CYARG_FEATURE_TEST_SYSTEM "TRUE" CACHE STRING "Include the test system")
endif()

if(YARG_DEVICE STREQUAL "RASPBERRY_PI_PICO")
# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
endif()

if(PICO_BOARD)
set(PICO_USE_FASTEST_SUPPORTED_CLOCK 1)
endif()

# The PICO SDK will also need CXX & ASM toolchains to assemble the firmware image
project(cyarg VERSION 0.1.0 LANGUAGES C CXX ASM)

if (PICO_BOARD)
# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()
endif()

add_executable(cyarg 
    common.h
    print.h
    chunk.h
    chunk.c
    main.c
    memory.h
    memory.c
    debug.h
    debug.c
    value.h
    value.c
    routine.h
    routine.c
    vm.h
    vm.c
    ast.h
    ast.c
    parser.h
    parser.c
    compiler.h
    compiler.c
    scanner.h
    scanner.c
    object.h
    object.c
    table.h
    table.c
    native.h
    native.c
    builtin.h
    builtin.c
    channel.h
    channel.c
    yargtype.h
    yargtype.c
    platform_hal.h
    platform_hal.c
    sync_group.h
    sync_group.c
    fs/fs.h
    )

if (CYARG_FEATURE_FILESYSTEM STREQUAL "LITTLEFS")
target_sources(cyarg
    PRIVATE
      fs/fs_littlefs.c
      ../hostyarg/littlefs/lfs.c
      ../hostyarg/littlefs/lfs_util.c)
if (YARG_DEVICE STREQUAL "RASPBERRY_PI_PICO")
target_sources(cyarg
    PRIVATE
      fs/pico_flash_fs.h
      fs/pico_lfs_hal.h
      fs/pico_lfs_hal.c)
endif()
elseif(CYARG_FEATURE_FILESYSTEM STREQUAL "CSTDLIB")
target_sources(cyarg
    PRIVATE
      fs/fs_stdlib.c)
endif()

if (CYARG_FEATURE_TEST_SYSTEM STREQUAL "TRUE")
target_sources(cyarg
    PRIVATE
      test-system/testBuiltin.h
      test-system/testBuiltin.cpp
      test-system/testSystem.h
      test-system/testSystem.cpp
      test-system/testIntrinsics.hpp
      )

add_compile_definitions(CYARG_FEATURE_TEST_SYSTEM)
endif()

if (CYARG_FEATURE_INTERACTIVE_TRACE STREQUAL "TRUE")
add_compile_definitions(DEBUG_TRACE_EXECUTION)
add_compile_definitions(DEBUG_AST_PARSE)
add_compile_definitions(DEBUG_STRESS_GC)
endif()

if (CYARG_FEATURE_DEBUG_LOG_GC STREQUAL "TRUE")
add_compile_definitions(DEBUG_LOG_GC)
endif()

if (YARG_DEVICE STREQUAL "RASPBERRY_PI_PICO")
add_compile_definitions(CYARG_PICO_TARGET)
add_compile_definitions(CYARG_FEATURE_SELF_HOSTED_REPL)
add_compile_definitions(CYARG_PICO_STDLIB)
add_compile_definitions(CYARG_PICO_SDK_SYNC)
add_compile_definitions(PICO_STACK_SIZE=0x1000)
elseif(YARG_DEVICE STREQUAL "GENERIC_HOST")
add_compile_definitions(CYARG_FEATURE_HOSTED_REPL)
add_compile_definitions(CYARG_PTHREADS_SYNC)
endif()


if (PICO_BOARD)
# Setup the firmware image and details of the executable it will host

pico_set_program_name(cyarg "cyarg")
pico_set_program_version(cyarg "0.1.0")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(cyarg 1)
pico_enable_stdio_usb(cyarg 1)

# Add the standard library to the build
target_link_libraries(cyarg
        hardware_pio
        hardware_flash
        pico_stdlib
        pico_multicore
        pico_sync
        )

# Add the standard include files to the build
target_include_directories(cyarg PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
)

pico_set_linker_script(cyarg ${CMAKE_SOURCE_DIR}/memmap_custom.ld)

pico_add_extra_outputs(cyarg)
endif()

if (YARG_DEVICE STREQUAL "GENERIC_HOST")
# Add the math library for the definition of pow()
target_link_libraries(cyarg m)
endif()
