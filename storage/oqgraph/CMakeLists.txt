FIND_PACKAGE(Boost 1.40.0)
IF(NOT Boost_FOUND)
  RETURN()
ENDIF()
INCLUDE_DIRECTORIES(BEFORE ${Boost_INCLUDE_DIRS})

# lp:756966 OQGRAPH on Win64 does not compile
IF(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(BOOST_OK 0)
ELSE()
# See if that works. On old gcc it'll fail because of -fno-rtti
CHECK_CXX_SOURCE_COMPILES(
"
#include <boost/config.hpp>
#include <boost/property_map/property_map.hpp>
int main() { return 0; }
" BOOST_OK)
ENDIF()

IF(BOOST_OK)
  ADD_DEFINITIONS(-DHAVE_OQGRAPH)
  IF(MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
  ELSE(MSVC)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated")
    STRING(REPLACE "-fno-exceptions" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    STRING(REPLACE "-fno-implicit-templates" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  ENDIF(MSVC)

  MYSQL_ADD_PLUGIN(oqgraph ha_oqgraph.cc graphcore.cc STORAGE_ENGINE
                   MODULE_ONLY)
ENDIF(BOOST_OK)
