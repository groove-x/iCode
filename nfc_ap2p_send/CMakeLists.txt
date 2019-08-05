cmake_minimum_required (VERSION 3.2)

project(nfc_ap2p_send)

include(../common.cmake)

set(CMAKE_C_FLAGS="-Wall")

include_directories(LIBRFAL_INC)

add_executable(nfc_ap2p_send
  nfc_ap2p/Src/main.c)

target_link_libraries(nfc_ap2p_send
  rfal
  pthread)
