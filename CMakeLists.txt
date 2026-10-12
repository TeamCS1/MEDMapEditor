cmake_minimum_required(VERSION 3.16)
project(MEd LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# With MinGW, link the C++ runtime and winpthread into every executable. Otherwise the program needs
# libstdc++-6.dll and libwinpthread-1.dll next to it or on the PATH, and Windows may pick up a different,
# older copy ("entry point not found") on a machine other than the one that built it.
if(MINGW)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

# Qt-free core: file formats (IDE, IPL, ...). The Qt UI will link against this.
add_library(med_core
    src/core/textutil.cpp
    src/core/col.cpp
    src/core/dff.cpp
    src/core/ide.cpp
    src/core/img.cpp
    src/core/ipl.cpp
    src/core/mapcheck.cpp
    src/core/pools.cpp
    src/core/region.cpp
    src/core/bookmarks.cpp
    src/core/extract.cpp src/core/original.cpp src/core/roadkit.cpp src/core/importer.cpp
    src/core/poollimits.cpp
    src/core/txd.cpp)
target_include_directories(med_core PUBLIC src/core)

enable_testing()
add_executable(test_parsers tests/test_parsers.cpp)
target_link_libraries(test_parsers PRIVATE med_core)
add_test(NAME parsers COMMAND test_parsers)

add_executable(test_img tests/test_img.cpp)
target_link_libraries(test_img PRIVATE med_core)
add_test(NAME img COMMAND test_img)

add_executable(test_history tests/test_history.cpp)
add_test(NAME history COMMAND test_history)

add_executable(test_menu tests/test_menu.cpp)
add_test(NAME menu COMMAND test_menu)

add_executable(test_mapcheck tests/test_mapcheck.cpp)
target_link_libraries(test_mapcheck PRIVATE med_core)
add_test(NAME mapcheck COMMAND test_mapcheck)

add_executable(test_raycast tests/test_raycast.cpp)
add_test(NAME raycast COMMAND test_raycast)

add_executable(test_rotation tests/test_rotation.cpp)
add_test(NAME rotation COMMAND test_rotation)

add_executable(test_region tests/test_region.cpp)
target_link_libraries(test_region PRIVATE med_core)
add_test(NAME region COMMAND test_region)

add_executable(test_bookmarks tests/test_bookmarks.cpp)
target_link_libraries(test_bookmarks PRIVATE med_core)
add_test(NAME bookmarks COMMAND test_bookmarks ${CMAKE_SOURCE_DIR}/MEdv032/bookmarks.ini)

add_executable(test_extract tests/test_extract.cpp)
target_link_libraries(test_extract PRIVATE med_core)
add_test(NAME extract COMMAND test_extract)
add_executable(test_original tests/test_original.cpp)
target_link_libraries(test_original PRIVATE med_core)
add_test(NAME original COMMAND test_original)
add_executable(test_roadkit tests/test_roadkit.cpp)
target_link_libraries(test_roadkit PRIVATE med_core)
add_test(NAME roadkit COMMAND test_roadkit)
add_executable(test_importer tests/test_importer.cpp)
target_link_libraries(test_importer PRIVATE med_core)
add_test(NAME importer COMMAND test_importer)
add_executable(road_ends tools/road_ends.cpp)
target_link_libraries(road_ends PRIVATE med_core)

add_executable(test_camera tests/test_camera.cpp)
add_test(NAME camera COMMAND test_camera)

add_executable(test_renderware tests/test_renderware.cpp)
target_link_libraries(test_renderware PRIVATE med_core)
add_test(NAME renderware COMMAND test_renderware)

# 3D map viewer (Win32 + fixed-function OpenGL, no extra dependencies).
if(WIN32)
    add_executable(med_viewer src/viewer/main.cpp)
    target_link_libraries(med_viewer PRIVATE med_core opengl32 glu32 gdi32 user32 comctl32 shell32 ole32 msimg32)
endif()

# Read-only tools for checking the library against a real game install.
add_executable(validate_gta3 tools/validate_gta3.cpp)
target_link_libraries(validate_gta3 PRIVATE med_core)
add_executable(dump_texture tools/dump_texture.cpp)
target_link_libraries(dump_texture PRIVATE med_core)

