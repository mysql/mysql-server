
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

FIND_PROGRAM(HEAT_EXECUTABLE heat ${WIX_DIR})
FIND_PROGRAM(CANDLE_EXECUTABLE candle ${WIX_DIR})
FIND_PROGRAM(LIGHT_EXECUTABLE light ${WIX_DIR})
 
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/create_msi.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
  @ONLY)
IF(CMAKE_GENERATOR MATCHES "Visual Studio")
 SET(CONFIG_PARAM "-DCMAKE_INSTALL_CONFIG_NAME=${CMAKE_CFG_INTDIR}")
ENDIF()
ADD_CUSTOM_TARGET(
  MSI
  COMMAND set VS_UNICODE_OUTPUT=
  COMMAND ${CMAKE_COMMAND} ${CONFIG_PARAM} -P  ${CMAKE_CURRENT_BINARY_DIR}/create_msi.cmake
)

