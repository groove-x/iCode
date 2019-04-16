cmake_minimum_required (VERSION 3.2)

project(librfal)

include(../common.cmake)

file(GLOB SOURCE1 "../rfal/Src/*.c")
file(GLOB SOURCE2 "../rfal/Src/st25r3911/*.c")
file(GLOB SOURCE3 "../platform/Src/*.c")
set(SOURCE ${SOURCE1} ${SOURCE2} ${SOURCE3})

add_library(rfal STATIC ${SOURCE})

target_link_libraries(rfal rt)
