# CMake generated Testfile for 
# Source directory: /home/agent-fighter-physics/src/fighter-physics
# Build directory: /home/agent-fighter-physics/src/fighter-physics/build-release
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test("fighter_physics_rollback_bench_smoke" "/home/agent-fighter-physics/src/fighter-physics/build-release/fighter_physics_rollback_bench" "--quick")
set_tests_properties("fighter_physics_rollback_bench_smoke" PROPERTIES  _BACKTRACE_TRIPLES "/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;30;add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;0;")
add_test("fighter_physics_smoke" "/home/agent-fighter-physics/src/fighter-physics/build-release/fighter_physics_smoke_test")
set_tests_properties("fighter_physics_smoke" PROPERTIES  _BACKTRACE_TRIPLES "/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;39;add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;42;fp_add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;0;")
add_test("fighter_physics_fixed" "/home/agent-fighter-physics/src/fighter-physics/build-release/fighter_physics_fixed_test")
set_tests_properties("fighter_physics_fixed" PROPERTIES  _BACKTRACE_TRIPLES "/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;39;add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;43;fp_add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;0;")
add_test("fighter_physics_vec2" "/home/agent-fighter-physics/src/fighter-physics/build-release/fighter_physics_vec2_test")
set_tests_properties("fighter_physics_vec2" PROPERTIES  _BACKTRACE_TRIPLES "/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;39;add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;44;fp_add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;0;")
add_test("fighter_physics_ground_movement" "/home/agent-fighter-physics/src/fighter-physics/build-release/fighter_physics_ground_movement_test")
set_tests_properties("fighter_physics_ground_movement" PROPERTIES  _BACKTRACE_TRIPLES "/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;39;add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;45;fp_add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;0;")
add_test("fighter_physics_air_movement" "/home/agent-fighter-physics/src/fighter-physics/build-release/fighter_physics_air_movement_test")
set_tests_properties("fighter_physics_air_movement" PROPERTIES  _BACKTRACE_TRIPLES "/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;39;add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;46;fp_add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;0;")
add_test("fighter_physics_world" "/home/agent-fighter-physics/src/fighter-physics/build-release/fighter_physics_world_test")
set_tests_properties("fighter_physics_world" PROPERTIES  _BACKTRACE_TRIPLES "/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;39;add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;47;fp_add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;0;")
add_test("fighter_physics_platform" "/home/agent-fighter-physics/src/fighter-physics/build-release/fighter_physics_platform_test")
set_tests_properties("fighter_physics_platform" PROPERTIES  _BACKTRACE_TRIPLES "/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;39;add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;48;fp_add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;0;")
add_test("fighter_physics_entity_contact" "/home/agent-fighter-physics/src/fighter-physics/build-release/fighter_physics_entity_contact_test")
set_tests_properties("fighter_physics_entity_contact" PROPERTIES  _BACKTRACE_TRIPLES "/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;39;add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;49;fp_add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;0;")
add_test("fighter_physics_state_hash" "/home/agent-fighter-physics/src/fighter-physics/build-release/fighter_physics_state_hash_test")
set_tests_properties("fighter_physics_state_hash" PROPERTIES  _BACKTRACE_TRIPLES "/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;39;add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;50;fp_add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;0;")
add_test("fighter_physics_jobs" "/home/agent-fighter-physics/src/fighter-physics/build-release/fighter_physics_jobs_test")
set_tests_properties("fighter_physics_jobs" PROPERTIES  _BACKTRACE_TRIPLES "/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;39;add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;51;fp_add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;0;")
add_test("fighter_physics_parallel_determinism" "/home/agent-fighter-physics/src/fighter-physics/build-release/fighter_physics_parallel_determinism_test")
set_tests_properties("fighter_physics_parallel_determinism" PROPERTIES  _BACKTRACE_TRIPLES "/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;39;add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;52;fp_add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;0;")
add_test("fighter_physics_rollback" "/home/agent-fighter-physics/src/fighter-physics/build-release/fighter_physics_rollback_test")
set_tests_properties("fighter_physics_rollback" PROPERTIES  _BACKTRACE_TRIPLES "/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;39;add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;53;fp_add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;0;")
add_test("fighter_physics_golden" "/home/agent-fighter-physics/src/fighter-physics/build-release/fighter_physics_golden_test")
set_tests_properties("fighter_physics_golden" PROPERTIES  _BACKTRACE_TRIPLES "/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;39;add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;54;fp_add_test;/home/agent-fighter-physics/src/fighter-physics/CMakeLists.txt;0;")
