cmake_minimum_required(VERSION 3.5)

project(mync LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_path(LIBURING_INCLUDE_DIR liburing.h PATH_SUFFIXES liburing)
find_library(LIBURING_LIBRARY NAMES uring)
if(NOT LIBURING_INCLUDE_DIR OR NOT LIBURING_LIBRARY)
    message(FATAL_ERROR "liburing not found")
endif()


add_executable(mync main.cpp
    stats.h
    epoll.h
    io_uring.h
    utils.h)


target_include_directories(mync PRIVATE ${LIBURING_INCLUDE_DIR})
target_link_libraries(mync ${LIBURING_LIBRARY})

include(GNUInstallDirs)
install(TARGETS mync
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
