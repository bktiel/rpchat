cmake_minimum_required(VERSION 3.22.1)

set(LIB_NAME rplib)
set(INCLUDE_DIR include)
set(SOURCE_DIR src)

project(${LIB_NAME})

file(GLOB LIB_HEADERS ${INCLUDE_DIR}/*.h)
file(GLOB LIB_SOURCE ${SOURCE_DIR}/*.c)


add_library(${LIB_NAME} STATIC
        ${LIB_HEADERS}
        ${LIB_SOURCE}
        )

target_include_directories(${LIB_NAME} PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/${LIB_NAME}>
        $<INSTALL_INTERFACE:include/${LIB_NAME}>
        )
target_include_directories(${LIB_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        )