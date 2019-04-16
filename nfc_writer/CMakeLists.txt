cmake_minimum_required (VERSION 3.2)

project (rfal)

message("build for ${ARCH}")
if (${ARCH} MATCHES "arm64")
  SET(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
  SET(CMAKE_C_FLAGS_RELEASE "-O3")
endif()

link_directories(../librfal/build/)

include_directories(
  ../utils/Inc
  ../platform/Inc
  ../rfal/Inc
  ../rfal/Src/st25r3911
  nfc_writer/Inc)


add_executable(nfc_writer
  nfc_writer/Src/main.c
  nfc_writer/Src/logger.c)

target_link_libraries(nfc_writer
  rfal
  pthread)
