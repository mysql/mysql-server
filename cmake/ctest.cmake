
INCLUDE(${MYSQL_CMAKE_SCRIPT_DIR}/cmake_parse_arguments.cmake)

MACRO(MY_ADD_TEST name)
  ADD_TEST(${name} ${name}-t)
ENDMACRO()

MACRO (MY_ADD_TESTS)
  MYSQL_PARSE_ARGUMENTS(ARG "LINK_LIBRARIES;EXT" "" ${ARGN})

  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include
                      ${CMAKE_SOURCE_DIR}/unittest/mytap)

  IF (NOT ARG_EXT)
    SET(ARG_EXT "c")
  ENDIF()

  FOREACH(name ${ARG_DEFAULT_ARGS})
    ADD_EXECUTABLE(${name}-t "${name}-t.${ARG_EXT}")
    TARGET_LINK_LIBRARIES(${name}-t mytap ${ARG_LINK_LIBRARIES})
    MY_ADD_TEST(${name})
  ENDFOREACH()
ENDMACRO()

