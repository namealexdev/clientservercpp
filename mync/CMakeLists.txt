cmake_minimum_required(VERSION 3.5)

project(mync LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mync main.cpp
    stats.h)

include(GNUInstallDirs)
install(TARGETS mync
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
