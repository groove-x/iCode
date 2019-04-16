# Bring headers into project
include_directories(Inc ../common/utils/Inc Src/st25r3911 Src ../platform/Inc /usr/include/)

#set(HEADER_FILES /home/pi/ST25R3911B/common/st_errno.h)

# Source files to build the project
file (GLOB SOURCE1 "Src/*.c")
file (GLOB SOURCE2 "Src/st25r3911/*.c")
file (GLOB SOURCE3 "../platform/Src/*.c")
set (SOURCE ${SOURCE1} ${SOURCE2} ${SOURCE3})

# Create the shared library
add_library (rfal_lib STATIC ${SOURCE})

target_link_libraries(
  rfal_lib
  pthread
  rt)

#set location for library installation
install(TARGETS rfal_lib DESTINATION .)
