cmake_minimum_required(VERSION 3.20)

project(DynamicHypergraphCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DHYP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

add_executable(DynamicHypergraphCore
        src/main.cpp
        src/Hypergraph.cpp
        src/Hypergraph.h
        src/Algorithm/NbrCD.h
        src/Algorithm/Final_nbrCoreDecomposition.cpp

        src/Algorithm_withInfo/Final_nbrCoreDecomposition.cpp

        src/Dynamic_Algorithm/DynamicModification.h
        src/Dynamic_Algorithm/EdgeAdd.cpp
        src/Dynamic_Algorithm/EdgeDelete.cpp
        src/Dynamic_Algorithm/util.h
        src/Dynamic_Algorithm/util.cpp

        src/graphTool/graphTool.cpp
        src/graphTool/graphTool.h
        src/graphTool/sortGraph.cpp
        src/graphTool/generateGraph.cpp
)

if(MSVC)
    target_compile_options(DynamicHypergraphCore PRIVATE /W4)
else()
    target_compile_options(DynamicHypergraphCore PRIVATE
            -Wall
            -Wextra
            -pedantic-errors
            -Wconversion
            -Wsign-conversion
            -Wdouble-promotion
            -Wcast-align
            -Wformat=2
            -Wuninitialized
            -Wnull-dereference
            -Wnon-virtual-dtor
            -Woverloaded-virtual
            -Wold-style-cast
            -Wzero-as-null-pointer-constant
            -Wsuggest-override
            -fstack-protector-strong
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(DynamicHypergraphCore PRIVATE
                -Wno-self-assign-overloaded
                -Wno-self-move
        )
    endif()
endif()

if(DHYP_WARNINGS_AS_ERRORS)
    if(MSVC)
        target_compile_options(DynamicHypergraphCore PRIVATE /WX)
    else()
        target_compile_options(DynamicHypergraphCore PRIVATE -Werror)
    endif()
endif()
