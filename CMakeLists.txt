cmake_minimum_required(VERSION 3.18)
project(redbunny C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_definitions(${LLVM_DEFINITIONS})

if(LLVM_LINK_LLVM_DYLIB)
    set(LLVM_LIBS LLVM)
else()
    llvm_map_components_to_libnames(LLVM_LIBS core analysis support)
endif()

add_library(redb_runtime STATIC runtime/runtime.c)

add_executable(redb
    src/main.c
    src/util.c
    src/lexer.c
    src/parser.c
    src/include.c
    src/typecheck.c
    src/codegen.c
)

target_include_directories(redb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${LLVM_INCLUDE_DIRS}
)

if(MSVC)
    target_compile_options(redb PRIVATE /W3)
else()
    target_compile_options(redb PRIVATE -Wall -Wextra)
endif()
target_compile_definitions(redb PRIVATE
    REDB_STDLIB_PATH="${CMAKE_CURRENT_SOURCE_DIR}/lib"
)
if(NOT WIN32)
    target_compile_definitions(redb PRIVATE
        REDB_RUNTIME_PATH="$<TARGET_FILE:redb_runtime>"
    )
endif()
target_link_libraries(redb PRIVATE ${LLVM_LIBS} redb_runtime)

install(TARGETS redb RUNTIME DESTINATION bin)
install(TARGETS redb_runtime ARCHIVE DESTINATION lib)
install(DIRECTORY lib/ DESTINATION share/redb/lib
    FILES_MATCHING PATTERN "*.redb")

if(UNIX)
    enable_testing()
    add_test(NAME redb_suite
        COMMAND ${CMAKE_COMMAND} -E env REDB=$<TARGET_FILE:redb>
                bash ${CMAKE_SOURCE_DIR}/tests/run.sh)
endif()
