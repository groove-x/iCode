#use the shared library
link_directories(../build/rfal)

#Bring headers into project
include_directories(
  ../common/utils/Inc
  ../platform/Inc
  ../rfal/Inc
  ../rfal/Src/st25r3911
  /usr/include
  iCodeDemo/Inc)

#sources
set (SOURCES iCodeDemo/Src/exampleNFC.c iCodeDemo/Src/logger.c)
add_executable(exampleNFC ${SOURCES})
target_link_libraries(exampleNFC rfal_lib)
