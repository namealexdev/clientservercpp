cmake_minimum_required(VERSION 3.5)

project(mync LANGUAGES CXX)

# add_compile_options(-O2 -march=native -D_GNU_SOURCE)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# find_path(LIBURING_INCLUDE_DIR liburing.h PATH_SUFFIXES liburing)
# find_library(LIBURING_LIBRARY NAMES uring)
# if(NOT LIBURING_INCLUDE_DIR OR NOT LIBURING_LIBRARY)
#     message(FATAL_ERROR "liburing not found")
# endif()


add_executable(mync main.cpp
    stats.h
    epoll.h epoll.cpp
    # io_uring.h
    utils.h
    snowflakeidgen.h
    simple_flat_map.h
    utils.cpp)


# target_include_directories(mync PRIVATE ${LIBURING_INCLUDE_DIR})
# target_link_libraries(mync ${LIBURING_LIBRARY})

include(GNUInstallDirs)
install(TARGETS mync
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
