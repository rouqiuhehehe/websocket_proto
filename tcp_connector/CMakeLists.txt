cmake_minimum_required(VERSION 3.19)
project(TcpConnectionsByVarian LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_PREFIX_PATH "D:\\qt\\6.9.3\\mingw_64")
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/ui")
set(CMAKE_AUTOMOC_PATH_PREFIX "${CMAKE_SOURCE_DIR}/include")

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}"
            CACHE PATH "Install path prefix" FORCE
    )
endif ()

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif ()

if (CMAKE_BUILD_TYPE MATCHES "Debug")
    set(CMAKE_VERBOSE_MAKEFILE ON)
endif ()

set(QT_VERSION 6)
#set(REQUIRED_WIN_LIBS wtsapi32)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE=1 -finput-charset=UTF-8 -fexec-charset=UTF-8")

include(cmake/global.cmake)

include_directories(include)

add_subdirectory(3rd)
add_subdirectory(src)