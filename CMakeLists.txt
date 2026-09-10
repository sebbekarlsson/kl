cmake_minimum_required(VERSION 3.20)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

set(PROJ_NAME kl)

project(${PROJ_NAME})

######### DEBUG
set(CMAKE_BUILD_TYPE RelWithDebInfo)


######## keep assert() calls
string(REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
#############################

SET(${PROJ_NAME}_COMPILE_FLAGS "-fno-strict-aliasing -g -Wall ${CMAKE_CXX_FLAGS}")

SET(CMAKE_CXX_FLAGS ${${PROJ_NAME}_COMPILE_FLAGS}) 
SET(CMAKE_C_FLAGS ${${PROJ_NAME}_COMPILE_FLAGS})


file(
  GLOB_RECURSE
  ${PROJ_NAME}_SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

add_library(${PROJ_NAME} STATIC ${${PROJ_NAME}_SOURCE_FILES})
target_include_directories(${PROJ_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${PROJ_NAME} PUBLIC m pthread atomic)
target_compile_options(${PROJ_NAME} PUBLIC -Wall -mavx2 -mfma)




##################
file(GLOB_RECURSE ${PROJ_NAME}_TEMPLATE_IMPL_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/templates/*.c
)

file(GLOB_RECURSE ${PROJ_NAME}_TEMPLATE_HEAD_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJ_NAME}/templates/*.h
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/macros_generated.timestamp
  COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR} && python3 ./scripts/generate_macros.py
  COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/macros_generated.timestamp
  DEPENDS 
    ${${PROJ_NAME}_TEMPLATE_HEAD_FILES}
    ${${PROJ_NAME}_TEMPLATE_IMPL_FILES}
)

add_custom_target(generate_macros DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/macros_generated.timestamp)
add_dependencies(${PROJ_NAME} generate_macros)

##################


install(TARGETS ${PROJ_NAME} DESTINATION lib)

function (add_subproject SUB_DIR SUB_NAME)
  if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${SUB_DIR}/${SUB_NAME})
    make_directory(${CMAKE_CURRENT_SOURCE_DIR}/${SUB_DIR}/${SUB_NAME}/src)
    write_file(${CMAKE_CURRENT_SOURCE_DIR}/${SUB_DIR}/${SUB_NAME}/src/main.c "#include <stdio.h>\n\nint main(int argc, char* argv[]) {\n  printf(\"hello\\n\");\n  return 0;\n}")
  endif()
  file(GLOB ${SUB_NAME}_SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${SUB_DIR}/${SUB_NAME}/src/*.c)
  add_executable(${SUB_NAME} ${${SUB_NAME}_SOURCE_FILES})
  target_link_libraries(${SUB_NAME} PUBLIC ${PROJ_NAME})
  target_include_directories(${SUB_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/${SUB_DIR}/${SUB_NAME}/include)
endfunction()


# The executables under executables/ are only interesting when kl is being
# developed on its own. When kl is consumed (add_subdirectory / FetchContent)
# they would otherwise be pulled into the parent project's `all` target, so
# they default to off in that case. A consumer can still opt in with
# -DKL_BUILD_EXECUTABLES=ON.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(KL_BUILD_EXECUTABLES_DEFAULT ON)
else()
  set(KL_BUILD_EXECUTABLES_DEFAULT OFF)
endif()

option(KL_BUILD_EXECUTABLES "Build kl's own executables (test, kl_playground, kl_bench)" ${KL_BUILD_EXECUTABLES_DEFAULT})

if (KL_BUILD_EXECUTABLES)
  add_subproject(executables test)
  add_subproject(executables kl_playground)
  add_subproject(executables kl_bench)
  add_subproject(executables kl_profile)
  target_include_directories(kl_bench PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/executables/test/include)
  # Reuse the test suites for benchmarking: the test functions referenced via
  # TEST_ENTRIES are defined in executables/test/src/test_*.c, which are otherwise
  # only compiled into the `test` executable. Glob them in (excludes main.c, which
  # carries the test runner's own main()).
  file(GLOB KL_TEST_SUITE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/executables/test/src/test_*.c)
  target_sources(kl_bench PRIVATE ${KL_TEST_SUITE_FILES})
endif()
