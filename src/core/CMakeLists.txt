file(GLOB SRCS *.c *.h)

add_subdirectory(mappers)
add_library(core ${SRCS})
target_link_libraries(core mappers)

#target_link_libraries(${EXE_NAME} m)

