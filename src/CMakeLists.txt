file(GLOB SRCS *.c)
file(GLOB INCS *.h)

find_package(SDL REQUIRED)
include_directories(${SDL_INCLUDE_DIR})

add_subdirectory(controllers)
add_subdirectory(mappers)
add_executable(${EXE_NAME} ${SRCS} ${INCS})
target_link_libraries(${EXE_NAME} m)
target_link_libraries(${EXE_NAME} controllers mappers)
target_link_libraries(${EXE_NAME} ${SDL_LIBRARY})
