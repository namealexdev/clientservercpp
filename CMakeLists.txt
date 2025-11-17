cmake_minimum_required(VERSION 3.5)

project(tcplibuse LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# using my lib
add_subdirectory(netlib)

# add_executable(tcplibuse main.cpp)
# target_link_libraries(tcplibuse netlib)
# include(GNUInstallDirs)

# install(TARGETS client
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
# )

add_executable(client tcpclient.cpp ${COMMON_SOURCES})
target_link_libraries(client netlib)

add_executable(server tcpserver.cpp ${COMMON_SOURCES})
target_link_libraries(server netlib)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address") # Для C++
# set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

# set (CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_STATIC_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address -lasan")
