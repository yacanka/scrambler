cmake_minimum_required(VERSION 3.22)
project(zip_header_tool VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

function(enable_project_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# GUI'den bagimsiz donusum cekirdegi.
add_library(scrambler_core STATIC src/zip_utils.c)
target_include_directories(scrambler_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
enable_project_warnings(scrambler_core)

option(SCRAMBLER_BUILD_APP "Build the raylib desktop application" ON)

if(SCRAMBLER_BUILD_APP)
    find_package(raylib 6.0 QUIET)

    if(NOT TARGET raylib)
        include(FetchContent)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
        set(SUPPORT_MODULE_RAUDIO OFF CACHE BOOL "" FORCE)
        set(SUPPORT_MODULE_RMODELS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG dbc56a87da87d973a9c5baa4e7438a9d20121d28
        )
        FetchContent_MakeAvailable(raylib)
    endif()

    add_executable(zip_header_tool
        src/main.c
        src/app.c
        src/ui.c
        src/system_font.c
    )
    target_include_directories(zip_header_tool PRIVATE src)
    target_link_libraries(zip_header_tool PRIVATE scrambler_core raylib)
    enable_project_warnings(zip_header_tool)

    if(WIN32 AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(zip_header_tool PRIVATE -mwindows)
    elseif(MSVC)
        target_link_options(zip_header_tool PRIVATE
            /SUBSYSTEM:WINDOWS
            /ENTRY:mainCRTStartup
        )
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(test_zip_utils tests/test_zip_utils.c)
    target_link_libraries(test_zip_utils PRIVATE scrambler_core)
    enable_project_warnings(test_zip_utils)
    add_test(NAME zip_utils_tests COMMAND test_zip_utils)
endif()
