cmake_minimum_required(VERSION 3.28)
project(fighter_physics VERSION 0.0.1 LANGUAGES CXX)

# A from-scratch, deterministic, parallel 2D physics and movement engine for platform fighters.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(fighter_physics src/version.cpp)
add_library(fp::fighter_physics ALIAS fighter_physics)
target_include_directories(fighter_physics PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(fighter_physics PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(fighter_physics_smoke_test tests/smoke_test.cpp)
  target_link_libraries(fighter_physics_smoke_test PRIVATE fp::fighter_physics)
  add_test(NAME fighter_physics_smoke COMMAND fighter_physics_smoke_test)
endif()
