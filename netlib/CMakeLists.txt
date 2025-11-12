cmake_minimum_required(VERSION 3.14)

project(netlib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(netlib SHARED
  libinclude/netlib.h
  libinclude/stats.h
  libinclude/iclient.h
  libinclude/iserver.h

  stats.cpp
  netlib.cpp

  const.h
  epoll.h epoll.cpp
  server.h server.cpp
  client.h client.cpp
  utils.h utils.cpp
)

target_include_directories(netlib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/
)
