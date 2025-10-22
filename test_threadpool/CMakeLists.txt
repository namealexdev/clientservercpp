cmake_minimum_required(VERSION 3.5)

project(test_threadpool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_threadpool main.cpp)

include(GNUInstallDirs)
install(TARGETS test_threadpool
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
