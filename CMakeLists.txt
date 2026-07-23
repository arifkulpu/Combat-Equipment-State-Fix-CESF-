cmake_minimum_required(VERSION 3.21)
project("CombatEquipmentStateFix" VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(COMMONLIB_BUILD_TESTS OFF CACHE BOOL "" FORCE)

set(SKYRIM_SUPPORT_AE ON)
set(SKYRIM_SUPPORT_SE ON)
set(SKYRIM_SUPPORT_VR ON)

include(FetchContent)
FetchContent_Declare(
    CommonLibSSE-NG
    GIT_REPOSITORY https://github.com/CharmedBaryon/CommonLibSSE-NG.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(CommonLibSSE-NG)

add_library(${PROJECT_NAME} SHARED
    src/main.cpp
    src/PCH.h
    src/SKSEMenuFramework.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    CommonLibSSE::CommonLibSSE
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)
