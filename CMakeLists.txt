cmake_minimum_required(VERSION 3.10)
project(egl_dma_test)

set(SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/src)

FILE(GLOB_RECURSE HeaderFiles ${SOURCE_DIR}/dma_buffer.hpp ${SOURCE_DIR}/logging.hpp)
FILE(GLOB_RECURSE SourceFiles ${SOURCE_DIR}/dma_buffer.cpp)

include_directories(${SOURCE_DIR})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os")

set(Libs EGL GLESv2 GLESv1_CM log)

add_library(utils ${SourceFiles})
target_link_libraries(utils PUBLIC ${Libs})
target_include_directories(utils PUBLIC ${SOURCE_DIR})

add_executable(main ${SOURCE_DIR}/main.cpp)
target_link_libraries(main PRIVATE utils)

install(TARGETS main DESTINATION ${CMAKE_INSTALL_PREFIX})