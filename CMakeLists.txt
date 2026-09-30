cmake_minimum_required(VERSION 3.20)
project(MAJEED LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT WIN32)
    message(FATAL_ERROR "MAJEED AEX build requires Windows/MSVC.")
endif()

add_subdirectory(extern)

set(MAJEED_AE_DIR   "${CMAKE_CURRENT_SOURCE_DIR}/ae")
set(MAJEED_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core")
set(PIPL_R          "${MAJEED_AE_DIR}/Majeed_PiPL.r")
set(PIPL_RR         "${CMAKE_CURRENT_BINARY_DIR}/Majeed_PiPL.rr")
set(PIPL_RRC        "${CMAKE_CURRENT_BINARY_DIR}/Majeed_PiPL.rrc")
set(PIPL_RC         "${CMAKE_CURRENT_BINARY_DIR}/Majeed_PiPL.rc")
set(PIPL_SCRIPT     "${CMAKE_CURRENT_BINARY_DIR}/GenerateMajeedPiPL.cmd")
set(PIPLTOOL        "${AESDK_ROOT}/Resources/PiPLtool.exe")

if(NOT EXISTS "${PIPL_R}")
    message(FATAL_ERROR "PiPL source not found: ${PIPL_R}")
endif()
if(NOT EXISTS "${PIPLTOOL}")
    message(FATAL_ERROR "PiPLtool.exe not found at: ${PIPLTOOL}")
endif()

# Adobe's own Windows SDK projects use this exact sequence:
#   cl /EP Majeed_PiPL.r -> .rr
#   PiPLTool .rr -> .rrc
#   cl /D MSWindows /EP .rrc -> .rc
# Use a generated .cmd file so Windows redirection and quoting are handled by
# cmd.exe exactly as they are in the SDK's Visual Studio projects.
file(WRITE "${PIPL_SCRIPT}" [=[
@echo off
setlocal
set "CL_EXE=@CL_EXE@"
set "SDK_HEADERS=@SDK_HEADERS@"
set "PIPL_IN=@PIPL_IN@"
set "PIPL_RR=@PIPL_RR@"
set "PIPLTOOL=@PIPLTOOL@"
set "PIPL_RRC=@PIPL_RRC@"
set "PIPL_RC=@PIPL_RC@"

"%CL_EXE%" /nologo /I "%SDK_HEADERS%" /EP "%PIPL_IN%" > "%PIPL_RR%"
if errorlevel 1 exit /b %errorlevel%

"%PIPLTOOL%" "%PIPL_RR%" "%PIPL_RRC%"
if errorlevel 1 exit /b %errorlevel%

"%CL_EXE%" /nologo /D "MSWindows" /EP "%PIPL_RRC%" > "%PIPL_RC%"
if errorlevel 1 exit /b %errorlevel%

if not exist "%PIPL_RC%" exit /b 2
exit /b 0
]=])

file(READ "${PIPL_SCRIPT}" PIPL_SCRIPT_TEXT)
string(REPLACE "@CL_EXE@" "${CMAKE_C_COMPILER}" PIPL_SCRIPT_TEXT "${PIPL_SCRIPT_TEXT}")
string(REPLACE "@SDK_HEADERS@" "${AESDK_ROOT}/Headers" PIPL_SCRIPT_TEXT "${PIPL_SCRIPT_TEXT}")
string(REPLACE "@PIPL_IN@" "${PIPL_R}" PIPL_SCRIPT_TEXT "${PIPL_SCRIPT_TEXT}")
string(REPLACE "@PIPL_RR@" "${PIPL_RR}" PIPL_SCRIPT_TEXT "${PIPL_SCRIPT_TEXT}")
string(REPLACE "@PIPLTOOL@" "${PIPLTOOL}" PIPL_SCRIPT_TEXT "${PIPL_SCRIPT_TEXT}")
string(REPLACE "@PIPL_RRC@" "${PIPL_RRC}" PIPL_SCRIPT_TEXT "${PIPL_SCRIPT_TEXT}")
string(REPLACE "@PIPL_RC@" "${PIPL_RC}" PIPL_SCRIPT_TEXT "${PIPL_SCRIPT_TEXT}")
file(WRITE "${PIPL_SCRIPT}" "${PIPL_SCRIPT_TEXT}")

add_custom_command(
    OUTPUT "${PIPL_RC}"
    COMMAND cmd.exe /C call "${PIPL_SCRIPT}"
    DEPENDS "${PIPL_R}" "${PIPLTOOL}"
    VERBATIM
    COMMENT "Generating Majeed_PiPL.rc"
)

file(GLOB MAJEED_AE_SOURCES CONFIGURE_DEPENDS "${MAJEED_AE_DIR}/*.cpp")
file(GLOB MAJEED_CORE_SOURCES CONFIGURE_DEPENDS "${MAJEED_CORE_DIR}/*.cpp")

add_library(MAJEED SHARED
    "${PIPL_RC}"
    ${MAJEED_AE_SOURCES}
    ${MAJEED_CORE_SOURCES}
)

add_dependencies(MAJEED AESDK)

target_include_directories(MAJEED PRIVATE
    "${MAJEED_AE_DIR}"
    "${MAJEED_CORE_DIR}"
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

set(PLUGIN_FOLDER
    "C:/Program Files/Adobe/Adobe After Effects 2023/Support Files/Plug-ins/"
    CACHE STRING "After Effects plugin folder"
)
install(TARGETS MAJEED DESTINATION "${PLUGIN_FOLDER}")
