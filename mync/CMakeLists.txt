cmake_minimum_required(VERSION 3.5)

project(mync LANGUAGES CXX)

# add_compile_options(-O2 -march=native -D_GNU_SOURCE)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# find_path(LIBURING_INCLUDE_DIR liburing.h PATH_SUFFIXES liburing)
# find_library(LIBURING_LIBRARY NAMES uring)
# if(NOT LIBURING_INCLUDE_DIR OR NOT LIBURING_LIBRARY)
#     message(FATAL_ERROR "liburing not found")
# endif()

# Для статической линковки стандартных библиотек
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")

# Или полная статическая линковка
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")



add_executable(mync main.cpp
    stats.h
    epollserver.h epollserver.cpp
    # io_uring.h
    utils.h utils.cpp
    # snowflakeidgen.h
    # simple_flat_map.h
    )


# target_include_directories(mync PRIVATE ${LIBURING_INCLUDE_DIR})
# target_link_libraries(mync ${LIBURING_LIBRARY})

target_link_libraries(mync PRIVATE Threads::Threads)

include(GNUInstallDirs)
install(TARGETS mync
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Для C++ проектов
set_target_properties(mync PROPERTIES
    LINK_SEARCH_START_STATIC 1
    LINK_SEARCH_END_STATIC 1
)
