cmake_minimum_required(VERSION 3.28)
project(fighter_physics VERSION 0.0.1 LANGUAGES CXX)

# A from-scratch, deterministic, parallel 2D physics and movement engine for platform fighters.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(fighter_physics src/version.cpp src/movement.cpp src/world.cpp src/state.cpp src/jobs.cpp)
add_library(fp::fighter_physics ALIAS fighter_physics)
find_package(Threads REQUIRED)  # the standard thread library, for the job system
target_link_libraries(fighter_physics PUBLIC Threads::Threads)
target_include_directories(fighter_physics PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(fighter_physics PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# The rollback benchmark: not a test; the CTest entry only checks that it runs.
add_executable(fighter_physics_rollback_bench bench/rollback_bench.cpp)
target_link_libraries(fighter_physics_rollback_bench PRIVATE fp::fighter_physics)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(fighter_physics_rollback_bench PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(CTest)
if(BUILD_TESTING)
  add_test(NAME fighter_physics_rollback_bench_smoke COMMAND fighter_physics_rollback_bench --quick)

  # Plain CTest executables, one per tests/<name>_test.cpp.
  function(fp_add_test name)
    add_executable(fighter_physics_${name}_test tests/${name}_test.cpp)
    target_link_libraries(fighter_physics_${name}_test PRIVATE fp::fighter_physics)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
      target_compile_options(fighter_physics_${name}_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    target_compile_definitions(fighter_physics_${name}_test PRIVATE
      FP_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/golden")
    add_test(NAME fighter_physics_${name} COMMAND fighter_physics_${name}_test)
  endfunction()

  fp_add_test(smoke)
  fp_add_test(fixed)
  fp_add_test(vec2)
  fp_add_test(ground_movement)
  fp_add_test(air_movement)
  fp_add_test(world)
  fp_add_test(platform)
  fp_add_test(stress)
  fp_add_test(placeable)
  fp_add_test(modifier)
  fp_add_test(world_golden)
  fp_add_test(entity_contact)
  fp_add_test(state_hash)
  fp_add_test(jobs)
  fp_add_test(parallel_determinism)
  fp_add_test(rollback)
  fp_add_test(determinism_trace)
  add_test(NAME fighter_physics_no_float COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-no-float)
  fp_add_test(golden)
endif()
