project(lexlib)

file(GLOB SRCS ${CMAKE_CURRENT_LIST_DIR}/*.cxx)
add_library(lexlib STATIC ${SRCS})
target_include_directories(lexlib PUBLIC ${CMAKE_SOURCE_DIR}/lexlib)
target_include_directories(lexlib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_include_directories(lexlib PUBLIC ${CMAKE_SOURCE_DIR}/include/scintilla)
