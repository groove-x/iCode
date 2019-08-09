cmake_minimum_required (VERSION 3.2)

project(nfc_ap2p)

include(../common.cmake)

set(CMAKE_C_FLAGS="-Wall")

include_directories(LIBRFAL_INC)

add_executable(demo
  demo/Src/main.c)

target_link_libraries(demo
  rfal
  pthread)
