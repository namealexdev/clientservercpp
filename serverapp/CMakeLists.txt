cmake_minimum_required(VERSION 3.5)

project(serverapp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# netlib
# /home/forte/DEV/mynetworkapp/netlib/build/Desktop-Release
add_subdirectory(../netlib build/Desktop-Release)

add_executable(serverapp main.cpp)

target_link_libraries(serverapp netlib)

include(GNUInstallDirs)
install(TARGETS serverapp
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)


