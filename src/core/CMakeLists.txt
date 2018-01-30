file(GLOB SRCS *.c)

find_package(SDL REQUIRED)
include_directories(${SDL_INCLUDE_DIR})

add_subdirectory(mappers)
add_library(core ${SRCS})
target_link_libraries(core mappers ${SDL_LIBRARY})

#target_link_libraries(${EXE_NAME} m)

