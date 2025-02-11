cmake_minimum_required(VERSION 3.31)
set(CMAKE_CONFIGURATION_TYPES "Debug" "Release")

set(PROJECT_NAME SimpleFS)
project(${PROJECT_NAME})

set(PROJECT_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(AFTER ${PROJECT_ROOT}/include)
set(HEADER_DIR ${PROJECT_ROOT}/include)
set(SOURCE_DIR ${PROJECT_ROOT}/src)
set(THIRDPARTY_DIR ${PROJECT_ROOT}/thirdparty)

# build thirdparties
configure_file(${PROJECT_ROOT}/CMakeLists_Thirdparty.txt.in ${THIRDPARTY_DIR}/CMakeLists.txt)

execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${THIRDPARTY_DIR})
if(result)
    message(FATAL_ERROR "Adding external projects failed: ${result}")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} --build . --config Debug --target ALL_BUILD
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${THIRDPARTY_DIR})
if(result)
    message(FATAL_ERROR "Building debug external projects failed: ${result}")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} --build . --config Release --target ALL_BUILD
        RESULT_VARIABLE result
        WORKING_DIRECTORY ${THIRDPARTY_DIR})
if(result)
    message(FATAL_ERROR "Building release external projects failed: ${result}")
endif()

set(MIMALLOC_ROOT ${THIRDPARTY_DIR}/mimalloc)
set(MIMALLOC_INCLUDE ${MIMALLOC_ROOT}/include/mimalloc-2.1)
set(MIMALLOC_LIB ${MIMALLOC_ROOT}/lib/mimalloc-2.1)

include_directories(AFTER ${MIMALLOC_INCLUDE})
link_directories(AFTER ${MIMALLOC_LIB})

add_library(MIMALLOC MODULE IMPORTED)
set_target_properties(MIMALLOC PROPERTIES IMPORTED_LOCATION_DEBUG "mimalloc-staticd.lib" IMPORTED_LOCATION_RELEASE "mimalloc-static.lib")

# LZ4
file(DOWNLOAD https://github.com/lz4/lz4/archive/refs/tags/v1.9.4.zip ${THIRDPARTY_DIR}/lz4.zip SHOW_PROGRESS)
file(ARCHIVE_EXTRACT INPUT ${THIRDPARTY_DIR}/lz4.zip DESTINATION ${THIRDPARTY_DIR})
include_directories(AFTER ${THIRDPARTY_DIR}/lz4-1.9.4/lib)
set(LZ4_DIR ${THIRDPARTY_DIR}/lz4-1.9.4/lib)
set(LZ4_HEADERS
    ${LZ4_DIR}/lz4.h
    ${LZ4_DIR}/lz4file.h
    ${LZ4_DIR}/lz4frame.h
    ${LZ4_DIR}/lz4frame_static.h
    ${LZ4_DIR}/lz4hc.h
    ${LZ4_DIR}/xxhash.h)

set(LZ4_SOURCES
    ${LZ4_DIR}/lz4.c
    ${LZ4_DIR}/lz4file.c
    ${LZ4_DIR}/lz4frame.c
    ${LZ4_DIR}/lz4hc.c
    ${LZ4_DIR}/xxhash.c)

########################################################################
# Sources
set(HEADERS
    "simplefs.h")

set(SOURCES
    "simplefs.cpp"
    "main.cpp")

source_group("include" FILES ${HEADERS})
source_group("include/lz4" FILES ${LZ4_HEADERS})
source_group("src" FILES ${SOURCES})
source_group("src/lz4" FILES ${LZ4_SOURCES})

set(FILES ${HEADERS} ${SOURCES} ${LZ4_HEADERS} ${LZ4_SOURCES})

set(OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}")

#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(${PROJECT_NAME} ${FILES})

if(MSVC)
    set(DEFAULT_CXX_FLAGS "/DWIN32 /D_WINDOWS /D_MBCS /W4 /WX- /nologo /fp:precise /arch:AVX /Zc:wchar_t /TP /Gd /std:c++20 /std:c11")
    if(MSVC_VERSION VERSION_LESS_EQUAL "1900")
        set(DEFAULT_CXX_FLAGS "${DEFAULT_CXX_FLAGS} /Zc:__cplusplus /std:c++latest")
    else()
        set(DEFAULT_CXX_FLAGS "${DEFAULT_CXX_FLAGS} /Zc:__cplusplus /std:c++20")
    endif()
    set(CMAKE_CXX_FLAGS "${DEFAULT_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1 /Gy /GR /GS /Gm- /EHsc")
    set(CMAKE_CXX_FLAGS_RELEASE "/MT /O2 /Oi /GL /GR /DNDEBUG /EHsc-")
    target_link_libraries(${PROJECT_NAME} MIMALLOC)

elseif(UNIX)
    set(DEFAULT_CXX_FLAGS "-Wall -O2 -std=c++20 -std=gnu++20 -march=x86-64-v3 -fno-exceptions")
    set(CMAKE_CXX_FLAGS "${DEFAULT_CXX_FLAGS}")
    target_link_libraries(${PROJECT_NAME} MIMALLOC)
elseif(APPLE)
elseif(APPLE)
endif()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME}
    PROPERTIES
        OUTPUT_NAME_DEBUG "${PROJECT_NAME}" OUTPUT_NAME_RELEASE "${PROJECT_NAME}"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(test)
