cmake_minimum_required (VERSION 3.2)

project(nfc_writer)

include(../common.cmake)

include_directories(LIBRFAL_INC
  nfc_writer/Inc)

add_executable(nfc_writer
  nfc_writer/Src/main.c
  nfc_writer/Src/logger.c)

target_link_libraries(nfc_writer
  rfal
  pthread)
