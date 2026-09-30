cmake_minimum_required(VERSION 3.20)
project(MAJEED LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE ON)

if(NOT WIN32)
    message(FATAL_ERROR "MAJEED AEX build requires Windows/MSVC.")
endif()

add_subdirectory(extern)

set(MAJEED_AE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ae)
set(MAJEED_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/core)
set(PLUGIN_FOLDER
    "C:/Program Files/Adobe/Adobe After Effects 2023/Support Files/Plug-ins/"
    CACHE STRING "After Effects plugin folder"
)

# Adobe PiPL resource generation follows the SDK's Windows project template.
set(PIPL_R ${MAJEED_AE_DIR}/Majeed_PiPL.r)
set(PIPL_RR ${CMAKE_CURRENT_BINARY_DIR}/Majeed_PiPL.rr)
set(PIPL_RRC ${CMAKE_CURRENT_BINARY_DIR}/Majeed_PiPL.rrc)
set(PIPL_RC ${CMAKE_CURRENT_BINARY_DIR}/Majeed_PiPL.rc)

add_custom_command(
    OUTPUT ${PIPL_RR}
    COMMAND cmd.exe /C "cl.exe /nologo /I\"${AESDK_ROOT}/Headers\" /EP \"${PIPL_R}\" > \"${PIPL_RR}\""
    DEPENDS ${PIPL_R}
    VERBATIM
)

add_custom_command(
    OUTPUT ${PIPL_RRC}
    COMMAND "${AFX_REZ}" ${PIPL_RR} ${PIPL_RRC}
    DEPENDS ${PIPL_RR}
    VERBATIM
)

add_custom_command(
    OUTPUT ${PIPL_RC}
    COMMAND cmd.exe /C "cl.exe /nologo /DMSWindows /EP \"${PIPL_RRC}\" > \"${PIPL_RC}\""
    DEPENDS ${PIPL_RRC}
    VERBATIM
)

file(GLOB MAJEED_AE_SOURCES CONFIGURE_DEPENDS "${MAJEED_AE_DIR}/*.cpp")
file(GLOB MAJEED_CORE_SOURCES CONFIGURE_DEPENDS "${MAJEED_CORE_DIR}/*.cpp")

add_library(MAJEED SHARED
    ${PIPL_RC}
    ${MAJEED_AE_SOURCES}
    ${MAJEED_CORE_SOURCES}
)

target_include_directories(MAJEED PRIVATE
    ${MAJEED_AE_DIR}
    ${MAJEED_CORE_DIR}
    ${AESDK_INCLUDE}
)

target_link_libraries(MAJEED PRIVATE AESDK)

target_compile_definitions(MAJEED PRIVATE MSWindows _WINDOWS WIN32)
target_compile_options(MAJEED PRIVATE /W3 /EHsc /permissive-)

set_target_properties(MAJEED PROPERTIES
    PREFIX ""
    OUTPUT_NAME "MAJEED"
    SUFFIX ".aex"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)

install(TARGETS MAJEED DESTINATION "${PLUGIN_FOLDER}")
