file(GLOB SRCS *.cpp *.h)

find_package(wxWidgets REQUIRED gl core base)
find_package(OpenGL)
include("${wxWidgets_USE_FILE}")

find_package(SDL REQUIRED)
include_directories(${SDL_INCLUDE_DIR})

add_executable(${EXE_NAME} ${SRCS})
target_link_libraries(${EXE_NAME} core ${wxWidgets_LIBRARIES} ${OPENGL_LIBRARIES} ${SDL_LIBRARY})
