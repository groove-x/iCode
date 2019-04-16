set(LIBRFAL_INC
  ../utils/Inc
  ../platform/Inc
  ../rfal/Inc
  ../rfal/Src/st25r3911
  ../rfal/Src)

message("build for ${ARCH}")
if (${ARCH} MATCHES "arm64")
  SET(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
  SET(CMAKE_C_FLAGS_RELEASE "-O3")
endif()

include_directories(
  ../utils/Inc
  ../platform/Inc
  ../rfal/Inc
  ../rfal/Src/st25r3911
  ../rfal/Src)

link_directories(../librfal/build/)
