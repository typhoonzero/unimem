cmake_minimum_required(VERSION 3.0)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/src/cmake")
set(THIRD_PARTY_PATH "${CMAKE_BINARY_DIR}/third_party" CACHE STRING
  "A path setting third party libraries download & build directories.")

project(unimem CXX C)

find_package(Git REQUIRED)
find_package(Threads REQUIRED)

include(flags)
include(generic)
include(gtest)

message(STATUS "CXX compiler: ${CMAKE_CXX_COMPILER}, version: "
  "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

include_directories(${CMAKE_SOURCE_DIR})

add_subdirectory("src/cpp/rw_lock")
add_subdirectory("src/cpp/allocators/bestfit_allocator")
add_subdirectory("src/cpp/allocators/uni_allocator")

