cmake_minimum_required (VERSION 3.2)

project(librfal)

set(CMAKE_CXX_FLAGS "-std=c11")
set(CMAKE_CXX_FLAGS "-O3")

file(GLOB SOURCE1 "Src/*.c")
file(GLOB SOURCE2 "Src/st25r3911/*.c")
file(GLOB SOURCE3 "platform/Src/*.c")
set(SOURCE ${SOURCE1} ${SOURCE2} ${SOURCE3})

include_directories(
  /usr/include
  common/utils/Inc
  platform/Inc
  rfal/Inc
  rfal/Src/st25r3911
  rfal/Src)

add_library(librfal STATIC ${SOURCE})

target_link_libraries(librfal
  rt)
