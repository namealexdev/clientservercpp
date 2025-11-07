cmake_minimum_required(VERSION 3.5)

project(tcplibuse LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# using my lib
add_subdirectory(netlib)

add_executable(tcplibuse main.cpp)

# link my lib
target_link_libraries(tcplibuse netlib)

include(GNUInstallDirs)
install(TARGETS tcplibuse
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
