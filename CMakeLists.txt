# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Set the Zephyr base directory
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

# Project name
project(segment_controller)

# Add source files
target_sources(app PRIVATE
    src/main.c
    src/network.c
    src/packet.c
    src/imu.c
    src/tmc9660.c
)

# Add include directories
target_include_directories(app PRIVATE
    include
    src
)
