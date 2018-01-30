file(GLOB SRCS *.cpp)

find_package(wxWidgets REQUIRED gl core base)
find_package(OpenGL)
include("${wxWidgets_USE_FILE}")

add_executable(${EXE_NAME} ${SRCS})
target_link_libraries(${EXE_NAME} core ${wxWidgets_LIBRARIES} ${OPENGL_LIBRARIES})
