cmake_minimum_required(VERSION 3.16)
project(bios-monitor VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(BIOS_MONITOR_NVML "Enable NVIDIA GPU monitoring via NVML" ON)

find_package(Curses REQUIRED)
find_package(Threads REQUIRED)

find_library(SENSORS_LIB sensors)
if(NOT SENSORS_LIB)
    message(WARNING "libsensors not found — thermal data will use sysfs only")
endif()

if(BIOS_MONITOR_NVML)
    find_library(NVML_LIB nvidia-ml)
    if(NVML_LIB)
        message(STATUS "NVML found: ${NVML_LIB}")
        add_compile_definitions(HAVE_NVML=1)
    else()
        message(STATUS "NVML not found — GPU monitoring via sysfs only")
    endif()
endif()

set(SOURCES
    src/main.c
    src/cpu.c
    src/memory.c
    src/gpu.c
    src/network.c
    src/disk.c
    src/sensors.c
    src/process.c
    src/data.c
    src/ui.c
    src/util.c
)

# util.c also implements common.h helpers

add_executable(bios-monitor ${SOURCES})
target_include_directories(bios-monitor PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(bios-monitor PRIVATE
    ${CURSES_LIBRARIES}
    Threads::Threads
    m
)

if(SENSORS_LIB)
    target_link_libraries(bios-monitor PRIVATE ${SENSORS_LIB})
    target_compile_definitions(bios-monitor PRIVATE HAVE_SENSORS=1)
endif()

if(NVML_LIB)
    target_link_libraries(bios-monitor PRIVATE ${NVML_LIB})
endif()

target_compile_options(bios-monitor PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Release>:-O2>
)

install(TARGETS bios-monitor DESTINATION bin)