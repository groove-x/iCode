cmake_minimum_required (VERSION 3.2)

project(librfal)

include(../common.cmake)

file(GLOB SOURCE1 "../common/rfal/Src/*.c")
file(GLOB SOURCE2 "../common/rfal/Src/st25r3911/*.c")
file(GLOB SOURCE3 "../common/platform/Src/*.c")
set(SOURCE ${SOURCE1} ${SOURCE2} ${SOURCE3})

add_library(rfal STATIC ${SOURCE})

target_link_libraries(rfal rt)
