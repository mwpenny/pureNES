file(GLOB SRCS *.c)
file(GLOB INCS *.h)

add_library(controllers ${SRCS} ${INCS})
