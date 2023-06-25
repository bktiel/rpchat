cmake_minimum_required(VERSION 3.22.1)
project(rpchat)

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set(CMAKE_CXX_STANDARD 17)
set(BINARY_NAME rpchat)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

find_program(CLANG_TIDY_PROG clang-tidy)
if (CLANG_TIDY_PROG)
    set(CMAKE_C_CLANG_TIDY ${CLANG_TIDY_PROG}
            "--checks=*, \
            -llvm-include-order, \
            -cppcoreguidelines-*, \
            -altera-struct-pack-align, \
            -fsanitize=thread, \
            -android-cloexec-open, \
            -hicpp-signed-bitwise, \
            -readability-magic-numbers, \
            -readability-function-cognitive-complexity, \
            -clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling, \
            -llvmlibc-restrict-system-libc-headers, \
            -hiccp-no-assembler"
            )
    set(CMAKE_C_FLAGS "-fsanitize=thread -g -Wall -Wextra -Wpedantic -Waggregate-return -Wwrite-strings -Wvla -Wfloat-equal")
else ()
    set(CMAKE_C_FLAGS "-g -Wall -Wextra -Wpedantic -Waggregate-return -Wwrite-strings -Wvla -Wfloat-equal")
endif ()


set(LIBRARIES_DIR lib)
set(LIBRARIES
        rplib)


file(GLOB BINARY_SOURCE src/*.c)
include_directories(include)

foreach(LIBRARY ${LIBRARIES})
    add_subdirectory("${LIBRARIES_DIR}/${LIBRARY}")
endforeach(LIBRARY)

add_executable(${BINARY_NAME} ${BINARY_SOURCE} include/basic_chat.h src/basic_chat.c src/networking.c include/networking.h)
target_link_libraries(${BINARY_NAME} ${LIBRARIES})


add_custom_command(TARGET ${BINARY_NAME}
        POST_BUILD
        COMMAND ./codechecker.sh
        )