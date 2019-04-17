cmake_minimum_required (VERSION 3.2)

project(nfc_writer)

include(../common.cmake)

set(CMAKE_C_FLAGS="-Wall")

include_directories(LIBRFAL_INC)

add_executable(nfc_writer
  nfc_writer/Src/main.c)

target_link_libraries(nfc_writer
  rfal
  pthread)
