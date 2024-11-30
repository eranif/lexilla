project(lexers_extra)

file(GLOB SRCS ${CMAKE_CURRENT_LIST_DIR}/*.cxx)
add_library(lexers_extra STATIC ${SRCS})
target_link_libraries(lexers_extra lexlib)
