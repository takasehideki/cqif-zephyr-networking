# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS
  "${CMAKE_CURRENT_SOURCE_DIR}/../../zephyr/share/zephyr-package/cmake"
)
project(tcp_accel)

set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../common)
target_include_directories(app PRIVATE ${COMMON_DIR})
target_sources(app PRIVATE src/main.c ${COMMON_DIR}/accel.c ${COMMON_DIR}/network.c)
