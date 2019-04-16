# CMakeLists text file of the project. 
# It is the input for cmake command, cmake is cross platform makefiles generator

cmake_minimum_required (VERSION 2.8.11)

#set (CMAKE_C_FLAGS "std=c99")

project (rfal)

message("build for ${ARCH}")
if (${ARCH} MATCHES "arm64")
  SET(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
  SET(CMAKE_C_FLAGS_RELEASE "-O3")
endif()

add_compile_options(-std=gnu99 -g)

add_subdirectory (rfal)
add_subdirectory (applications)



