cmake_minimum_required (VERSION 3.2)

project(librfal)

message("build for ${ARCH}")
if (${ARCH} MATCHES "arm64")
  SET(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
  SET(CMAKE_C_FLAGS_RELEASE "-O3")
endif()

file(GLOB SOURCE1 "rfal/Src/*.c")
file(GLOB SOURCE2 "rfal/Src/st25r3911/*.c")
file(GLOB SOURCE3 "platform/Src/*.c")
set(SOURCE ${SOURCE1} ${SOURCE2} ${SOURCE3})

include_directories(
  common/utils/Inc
  platform/Inc
  rfal/Inc
  rfal/Src/st25r3911
  rfal/Src)

add_library(rfal STATIC ${SOURCE})

target_link_libraries(rfal
  rt)
