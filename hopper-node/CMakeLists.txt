cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../components/hopper_protocol")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(tpb9000_hopper_node)
