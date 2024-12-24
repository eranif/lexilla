project(lexers_extra)

file(GLOB SRCS ${CMAKE_CURRENT_LIST_DIR}/*.cxx)
add_library(lexers_extra STATIC ${SRCS})
target_link_libraries(lexers_extra lexlib)
target_include_directories(lexers_extra PUBLIC "${CMAKE_SOURCE_DIR}/include")
set_target_properties(lexers_extra PROPERTIES POSITION_INDEPENDENT_CODE ON)

