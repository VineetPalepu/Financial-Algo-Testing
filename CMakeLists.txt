cmake_minimum_required(VERSION 3.0)
project(finance)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /diagnostics:caret")

if(MSVC)
    add_compile_options("/W4")
endif()


# Allow VS Code to find include directories for intellisense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB INCLUDES "include/*/include" "include")
include_directories(${INCLUDES})

file(GLOB SOURCES "src/*.cpp")



add_executable(${PROJECT_NAME} ${SOURCES})
