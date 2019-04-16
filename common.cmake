message("build for ${ARCH}")
if (${ARCH} MATCHES "arm64")
  SET(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
  SET(CMAKE_C_FLAGS_RELEASE "-O3")
endif()

include_directories(
  ../common/utils/Inc
  ../common/platform/Inc
  ../common/rfal/Inc
  ../common/rfal/Src/st25r3911
  ../common/rfal/Src)

link_directories(../librfal/build/)
