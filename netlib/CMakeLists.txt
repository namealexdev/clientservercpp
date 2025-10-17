cmake_minimum_required(VERSION 3.14)

project(netlib LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(netlib SHARED
  netlib.cpp
  netlib.h
  server.h
  client.h
  const.h
  queue.h
  threadpool.h
  utils.h utils.cpp
)

target_include_directories(netlib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/
)
