cmake_minimum_required(VERSION 3.10)
project(zip_header_tool C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

add_executable(zip_header_tool
    src/main.c
    src/ui.c
    src/font_finder.c
    src/zip_utils.c
)

if(TARGET SDL2::SDL2)
    target_link_libraries(zip_header_tool PRIVATE SDL2::SDL2)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(zip_header_tool PRIVATE SDL2::SDL2main)
    endif()
else()
    target_include_directories(zip_header_tool PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(zip_header_tool PRIVATE ${SDL2_LIBRARIES})
endif()

if(TARGET SDL2_ttf::SDL2_ttf)
    target_link_libraries(zip_header_tool PRIVATE SDL2_ttf::SDL2_ttf)
else()
    target_include_directories(zip_header_tool PRIVATE ${SDL2_TTF_INCLUDE_DIRS})
    target_link_libraries(zip_header_tool PRIVATE ${SDL2_TTF_LIBRARIES})
endif()

# ui.c icindeki sqrt/lround icin (Linux'ta genelde ayrica linklenmesi
# gerekir; macOS/Windows'ta zararsizdir).
if(UNIX)
    target_link_libraries(zip_header_tool PRIVATE m)
endif()

if(WIN32)
    # Windows'ta konsol penceresi acilmasin, sadece GUI penceresi gorunsun.
    set_target_properties(zip_header_tool PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Bagimsiz mantik testleri (SDL gerektirmez)
enable_testing()
add_executable(test_zip_utils EXCLUDE_FROM_ALL
    tests/test_zip_utils.c
    src/zip_utils.c
)
target_include_directories(test_zip_utils PRIVATE src)
add_test(NAME zip_utils_tests COMMAND test_zip_utils)
