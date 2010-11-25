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

  SET(OQGRAPH_PLUGIN_STATIC  "oqgraph")
  SET(OQGRAPH_PLUGIN_DYNAMIC "ha_oqgraph")
  SET(OQGRAPH_SOURCES ha_oqgraph.cc graphcore.cc)
  MYSQL_ADD_PLUGIN(oqgraph ${OQGRAPH_SOURCES} STORAGE_ENGINE)
ENDIF(BOOST_OK)
#error same fix here as in plugin.m4
