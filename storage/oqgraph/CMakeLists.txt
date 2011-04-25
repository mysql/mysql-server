CHECK_CXX_SOURCE_COMPILES(
"#include <boost/version.hpp>
#if BOOST_VERSION >= 104000
#else
#error oops
#endif
int main() { return 0; }" BOOST_OK)

IF(BOOST_OK)
  ADD_DEFINITIONS(-DHAVE_OQGRAPH)
  IF(MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
  ENDIF(MSVC)
  IF(CMAKE_CXX_FLAGS)
    STRING(REPLACE "-fno-exceptions" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    STRING(REPLACE "-fno-implicit-templates" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  ENDIF()

  MYSQL_ADD_PLUGIN(oqgraph ha_oqgraph.cc graphcore.cc STORAGE_ENGINE)
ENDIF(BOOST_OK)
