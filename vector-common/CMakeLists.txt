SET(VECTOR_COMMON_SOURCES
  vector_conversion.cc
  vector_operations.cc
)
ADD_CONVENIENCE_LIBRARY(vector_common ${VECTOR_COMMON_SOURCES})
TARGET_LINK_LIBRARIES(vector_common mysys)
