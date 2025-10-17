cmake_minimum_required(VERSION 3.5)

project(clientapp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(../netlib build/Desktop-Release)

add_executable(clientapp main.cpp)

target_link_libraries(clientapp netlib)

include(GNUInstallDirs)
install(TARGETS clientapp
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
