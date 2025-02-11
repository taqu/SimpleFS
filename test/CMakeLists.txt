
set(CMAKE_CONFIGURATION_TYPES "Debug" "Release")

set(PROJECT_NAME SimpleFSTest)
project(${PROJECT_NAME})

set(HEADERS ${PROJECT_ROOT}/simplefs.h "catch_amalgamated.hpp")
set(SOURCES ${PROJECT_ROOT}/simplefs.cpp "main.cpp;test.cpp;catch_amalgamated.cpp")

include_directories(AFTER ${CMAKE_CURRENT_SOURCE_DIR})

source_group("include" FILES ${HEADERS})
source_group("include/lz4" FILES ${LZ4_HEADERS})
source_group("src" FILES ${SOURCES})
source_group("src/lz4" FILES ${LZ4_SOURCES})

set(FILES ${HEADERS} ${SOURCES} ${LZ4_HEADERS} ${LZ4_SOURCES})

set(OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}")

add_executable(${PROJECT_NAME} ${FILES})

if(MSVC)
    set(DEFAULT_CXX_FLAGS "/DWIN32 /D_WINDOWS /D_MBCS /W4 /WX- /nologo /fp:precise /arch:AVX /Zc:wchar_t /TP /Gd /std:c++20 /std:c11")
    set(DEFAULT_CXX_FLAGS "${DEFAULT_CXX_FLAGS} /EHsc /Zc:__cplusplus /std:c++20")

    set(DEFAULT_CXX_FLAGS_DEBUG "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1 /Gy /GR- /GS /Gm-")
    set(DEFAULT_CXX_FLAGS_RELEASE "/MT /O2 /GL /GR- /DNDEBUG")
    set(DEFAULT_CXX_LINK_FLAGS_DEBUG "")
    set(DEFAULT_CXX_LINK_FLAGS_RELEASE "/LTCG")

    set(CMAKE_CXX_FLAGS "${DEFAULT_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "${DEFAULT_CXX_FLAGS_DEBUG}")
    set(CMAKE_CXX_FLAGS_RELEASE "${DEFAULT_CXX_FLAGS_RELEASE}")
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS_DEBUG "${DEFAULT_CXX_LINK_FLAGS_DEBUG}"
        LINK_FLAGS_RELEASE "${DEFAULT_CXX_LINK_FLAGS_RELEASE}")
    target_link_libraries(${PROJECT_NAME} MIMALLOC)

elseif(UNIX)
    set(DEFAULT_CXX_FLAGS "-Wall -O2 -std=c++20 -std=gnu++20 -march=x86-64-v3 -fno-exceptions")
    set(CMAKE_CXX_FLAGS "${DEFAULT_CXX_FLAGS}")
    target_link_libraries(${PROJECT_NAME} MIMALLOC)
elseif(APPLE)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME_DEBUG "${PROJECT_NAME}" OUTPUT_NAME_RELEASE "${PROJECT_NAME}")
