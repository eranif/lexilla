project(lexlib)

file(GLOB SRCS ${CMAKE_CURRENT_LIST_DIR}/*.cxx)
set(LEXILLA_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

add_library(lexlib STATIC ${SRCS})
target_include_directories(lexlib PUBLIC "${LEXILLA_ROOT}/lexlib")
target_include_directories(lexlib PUBLIC "${LEXILLA_ROOT}/include")
target_include_directories(lexlib PUBLIC "${LEXILLA_ROOT}/include/scintilla")
set_target_properties(lexlib PROPERTIES POSITION_INDEPENDENT_CODE ON)

