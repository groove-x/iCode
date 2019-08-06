cmake_minimum_required (VERSION 3.2)

project(nfc_ap2p_listener)

include(../common.cmake)

set(CMAKE_C_FLAGS="-Wall")

include_directories(LIBRFAL_INC)

add_executable(nfc_ap2p_listener
  nfc_ap2p_listener/Src/main.c)

target_link_libraries(nfc_ap2p_listener
  rfal
  pthread)
