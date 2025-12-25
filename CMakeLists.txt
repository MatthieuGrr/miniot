# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Extraire la version depuis git
execute_process(
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/get_version.sh
    OUTPUT_VARIABLE FIRMWARE_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Firmware version from git: ${FIRMWARE_VERSION}")

# Générer le fichier version.h
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/main/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
    @ONLY
)

# Use custom partition table
set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(miniot)
