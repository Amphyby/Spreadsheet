cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
project(Spreadsheet)

set(CMAKE_CXX_STANDARD 17)
if(MSVC)
  set(
    CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_CXX_FLAGS_DEBUG} /JMC"
  )
else()
  set(
    CMAKE_CXX_FLAGS
    "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wno-unused-parameter -Wno-implicit-fallthrough"
  )
endif()

set(ANTLR_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/antlr-4.7.2-complete.jar)
include(${CMAKE_CURRENT_SOURCE_DIR}/FindANTLR.cmake)

add_definitions(
  -DANTLR4CPP_STATIC
  -D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
)

set(WITH_STATIC_CRT OFF CACHE BOOL "Visual C++ static CRT for ANTLR" FORCE)
add_subdirectory(antlr4_runtime)

antlr_target(FormulaParser Formula.g4 LEXER PARSER LISTENER)

include_directories(
  ${ANTLR4_INCLUDE_DIRS}
  ${ANTLR_FormulaParser_OUTPUT_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/antlr4_runtime/runtime/src
)

file(GLOB sources
  *.cpp
  *.h
)

include(FetchContent)
FetchContent_Declare(
    PVS_CMakeModule
    GIT_REPOSITORY "https://github.com/viva64/pvs-studio-cmake-module.git"
    GIT_TAG        "master"
)
FetchContent_MakeAvailable(PVS_CMakeModule)
include("${pvs_cmakemodule_SOURCE_DIR}/PVS-Studio.cmake")

add_executable(
  spreadsheet
  ${ANTLR_FormulaParser_CXX_OUTPUTS}
  ${sources}
)

pvs_studio_add_target(TARGET spreadsheet.analyze ALL
                      OUTPUT FORMAT json
                      ANALYZE spreadsheet
                      MODE GA:1,2
                      LOG target.err
                      ARGS -e antlr4_runtime/)

target_link_libraries(spreadsheet antlr4_static)
if(MSVC)
  target_compile_options(antlr4_static PRIVATE /W0)
endif()

install(
  TARGETS spreadsheet
  DESTINATION bin
  EXPORT spreadsheet
)

set_directory_properties(PROPERTIES VS_STARTUP_PROJECT spreadsheet)
