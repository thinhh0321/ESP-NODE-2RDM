# Minimum CMake version
cmake_minimum_required(VERSION 3.5)

# Add external component directories
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/managed_components"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESP-NODE-2RDM)