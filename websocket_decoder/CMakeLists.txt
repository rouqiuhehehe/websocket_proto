cmake_minimum_required(VERSION 3.19)
project(websocket_decoder)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_FIND_DEBUG_MODE ON)

find_package(OpenSSL REQUIRED)
find_package(fmt REQUIRED)

aux_source_directory(. SOURCE_SRC)
add_executable(${PROJECT_NAME} ${SOURCE_SRC})
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::Crypto fmt::fmt)