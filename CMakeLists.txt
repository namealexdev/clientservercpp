cmake_minimum_required(VERSION 3.5)

project(tcplibuse LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# using my lib
add_subdirectory(netlib)

add_executable(client tcpclient.cpp ${COMMON_SOURCES})
target_link_libraries(client netlib)

# lockfree queue
find_package(Boost REQUIRED)
target_link_libraries(client Boost::boost)

add_executable(server tcpserver.cpp ${COMMON_SOURCES})
target_link_libraries(server netlib)

#tests
find_package(Boost REQUIRED COMPONENTS unit_test_framework)

add_executable(tests tests.cpp ${COMMON_SOURCES}
    tests.h)
target_link_libraries(tests
    netlib
    Boost::unit_test_framework
)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address") # Для C++
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")


# set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
# set (CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_STATIC_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address -lasan")
