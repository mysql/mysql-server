FIND_PACKAGE(Boost 1.40.0)
IF(NOT Boost_FOUND)
  RETURN()
ENDIF()
INCLUDE_DIRECTORIES(BEFORE ${Boost_INCLUDE_DIRS})
SET(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${Boost_INCLUDE_DIRS})

IF(MSVC)
  # lp:756966 OQGRAPH on Win64 does not compile
  IF (CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET(BOOST_OK 0)
  ELSE()
    SET(BOOST_OK 1)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
  ENDIF()
ELSE()
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated -fno-strict-aliasing")
  STRING(REPLACE "-fno-exceptions" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  STRING(REPLACE "-fno-implicit-templates" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
# See if that works. On old gcc it'll fail because of -fno-rtti
CHECK_CXX_SOURCE_COMPILES(
"
#include <boost/config.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/graph/adjacency_list.hpp>

using namespace boost;
struct VertexInfo { int id; };

namespace boost
{ 
  namespace graph
  {
    template<> struct internal_vertex_name<VertexInfo>
    { typedef multi_index::member<VertexInfo, int , &VertexInfo::id> type; };
  }
}

typedef adjacency_list<vecS, vecS, bidirectionalS, VertexInfo> Graph;
int main()
{
  graph_traits<Graph>::vertex_descriptor *orig;
  Graph *g;
  remove_vertex(*orig, *g);
  return 0;
}
" BOOST_OK)
ENDIF()

IF(BOOST_OK)
  ADD_DEFINITIONS(-DHAVE_OQGRAPH)

  MYSQL_ADD_PLUGIN(oqgraph ha_oqgraph.cc graphcore.cc STORAGE_ENGINE
                   MODULE_ONLY)
  SET(OQGRAPH_DEB_FILES "usr/lib/mysql/plugin/ha_oqgraph.so\n" PARENT_SCOPE)
ENDIF(BOOST_OK)
