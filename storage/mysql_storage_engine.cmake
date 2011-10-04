# MYSQL_STORAGE_ENGINE Macro creates a project to build storage engine
# library. 
#
# Parameters:
# engine - storage engine name.
# variable ENGINE_BUILD_TYPE should be set to "STATIC" or "DYNAMIC"
# Remarks:
# ${engine}_SOURCES  variable containing source files to produce the library must set before
# calling this macro
# ${engine}_LIBS variable containing extra libraries to link with may be set


MACRO(MYSQL_PLUGIN engine)
IF(NOT SOURCE_SUBLIBS)
  # Add common include directories
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)
  STRING(TOUPPER ${engine} engine)
  IF(${ENGINE_BUILD_TYPE} STREQUAL "STATIC")
    ADD_LIBRARY(${${engine}_LIB} ${${engine}_SOURCES})
    ADD_DEPENDENCIES(${${engine}_LIB} GenError)
    IF(${engine}_LIBS)
      TARGET_LINK_LIBRARIES(${${engine}_LIB} ${${engine}_LIBS})
    ENDIF(${engine}_LIBS)
    MESSAGE("build ${engine} as static library (${${engine}_LIB}.lib)")
  ELSEIF(${ENGINE_BUILD_TYPE} STREQUAL "DYNAMIC")
    ADD_DEFINITIONS(-DMYSQL_DYNAMIC_PLUGIN)
    ADD_LIBRARY(${${engine}_LIB} SHARED ${${engine}_SOURCES})
    TARGET_LINK_LIBRARIES (${${engine}_LIB}  mysqld)
    IF(${engine}_LIBS)
      TARGET_LINK_LIBRARIES(${${engine}_LIB} ${${engine}_LIBS})
    ENDIF(${engine}_LIBS)
    # Install the plugin
    INSTALL(TARGETS ${${engine}_LIB} DESTINATION lib/plugin COMPONENT runtime)
    MESSAGE("build ${engine} as DLL (${${engine}_LIB}.dll)")
  ENDIF(${ENGINE_BUILD_TYPE} STREQUAL "STATIC")
ENDIF(NOT SOURCE_SUBLIBS)
ENDMACRO(MYSQL_PLUGIN)

MACRO(MYSQL_STORAGE_ENGINE engine)
IF(NOT SOURCE_SUBLIBS)
  MYSQL_PLUGIN(${engine})
  INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/zlib ${CMAKE_SOURCE_DIR}/sql
                      ${CMAKE_SOURCE_DIR}/regex
                      ${CMAKE_SOURCE_DIR}/extra/yassl/include)
  IF(${ENGINE_BUILD_TYPE} STREQUAL "STATIC")
    ADD_DEFINITIONS(-DWITH_${engine}_STORAGE_ENGINE -DMYSQL_SERVER)
  ENDIF(${ENGINE_BUILD_TYPE} STREQUAL "STATIC")
ENDIF(NOT SOURCE_SUBLIBS)
ENDMACRO(MYSQL_STORAGE_ENGINE)
