
IF(NOT WIN32)
  RETURN()
ENDIF()

FIND_PATH(WIX_DIR heat.exe
   $ENV{WIX_DIR}/bin
   $ENV{ProgramFiles}/wix/bin
  "$ENV{ProgramFiles}/Windows Installer XML v3/bin"
  "$ENV{ProgramFiles}/Windows Installer XML v3.5/bin"
)

IF(NOT WIX_DIR)
  IF(NOT _WIX_DIR_CHECKED)
    SET(_WIX_DIR_CHECKED 1 CACHE INTERNAL "")
    MESSAGE(STATUS "Cannot find wix 3, installer project will not be generated")
  ENDIF()
  RETURN()
ENDIF()

# extra.wxs.in needs DATADIR_MYSQL_FILES and DATADIR_PERFORMANCE_SCHEMA_FILES, i.e
# Wix-compatible file lists for ${builddir}\sql\data\{mysql,performance_schema}

FOREACH(dir mysql performance_schema)
 FILE(GLOB files ${CMAKE_BINARY_DIR}/sql/data/${dir}/*)
 SET(filelist)
 FOREACH(f ${files})
   FILE(TO_NATIVE_PATH "${f}" file_native_path)
   GET_FILENAME_COMPONENT(file_name "${f}" NAME)
   SET(filelist 
"${filelist}
<File Id='${file_name}' Source='${file_native_path}'/>")
 ENDFOREACH()
 STRING(TOUPPER ${dir} DIR_UPPER)
 SET(DATADIR_${DIR_UPPER}_FILES "${filelist}")
ENDFOREACH()


FIND_PROGRAM(HEAT_EXECUTABLE heat ${WIX_DIR})
FIND_PROGRAM(CANDLE_EXECUTABLE candle ${WIX_DIR})
FIND_PROGRAM(LIGHT_EXECUTABLE light ${WIX_DIR})

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/create_msi.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
  @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/extra.wxs.in
  ${CMAKE_CURRENT_BINARY_DIR}/extra.wxs
  @ONLY
  )

IF(CMAKE_GENERATOR MATCHES "Visual Studio")
 SET(CONFIG_PARAM "-DCMAKE_INSTALL_CONFIG_NAME=${CMAKE_CFG_INTDIR}")
ENDIF()


ADD_CUSTOM_TARGET(
  MSI
  COMMAND set VS_UNICODE_OUTPUT=
  COMMAND ${CMAKE_COMMAND} 
  -DCPACK_WIX_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/CPackWixConfig.cmake
  -DCPACK_WIX_INCLUDE=${CMAKE_CURRENT_BINARY_DIR}/extra.wxs
  ${CONFIG_PARAM} 
  -P  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
)

ADD_CUSTOM_TARGET(
  MSI_ESSENTIALS
  COMMAND set VS_UNICODE_OUTPUT=
  COMMAND ${CMAKE_COMMAND} -DESSENTIALS=1 
  -DCPACK_WIX_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/CPackWixConfig.cmake
  -DCPACK_WIX_INCLUDE=${CMAKE_CURRENT_BINARY_DIR}/extra.wxs
  ${CONFIG_PARAM}
  -P  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
)

