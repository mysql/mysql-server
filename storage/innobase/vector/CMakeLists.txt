# storage/innobase/vector/CMakeLists.txt
# HNSW Vector Index module for InnoDB

SET(VECTOR_SOURCES
  vec0hnsw.cc
)

ADD_CONVENIENCE_LIBRARY(innobase_vector
  ${VECTOR_SOURCES}
)

TARGET_INCLUDE_DIRECTORIES(innobase_vector PRIVATE
  ${CMAKE_SOURCE_DIR}/storage/innobase/vector
  ${CMAKE_SOURCE_DIR}/storage/innobase/include
)
