IF(NOT MSVC)
  RETURN()
ENDIF()
IF(CMAKE_USING_VC_FREE_TOOLS)
  # No MFC, so it cannot be built
  RETURN()
ENDIF()

# We need MFC
FIND_PACKAGE(MFC)
IF(NOT MFC_FOUND)
  RETURN()
ENDIF()  

# MFC should be statically linked
SET(CMAKE_MFC_FLAG 1)

# Enable exception handling (avoids warnings)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/sql)
MYSQL_ADD_EXECUTABLE(upgrade_wizard
  upgrade.cpp upgradeDlg.cpp upgrade.rc
  COMPONENT Server)
TARGET_LINK_LIBRARIES(upgrade_wizard winservice)
# upgrade_wizard is Windows executable, set WIN32_EXECUTABLE so it does not
# create a console.
SET_TARGET_PROPERTIES(upgrade_wizard PROPERTIES WIN32_EXECUTABLE 1)

# Embed Vista "admin" manifest, since upgrade_wizard needs admin privileges
# to change service configuration. Due to a CMake bug http://www.vtk.org/Bug/view.php?id=11171
# it is not possible currenly to do it with linker flags. Work around is to use
# manifest tool mt.exe and embed the manifest post-build.
GET_TARGET_PROPERTY(upgrade_wizard_location upgrade_wizard LOCATION)
ADD_CUSTOM_COMMAND(
  TARGET upgrade_wizard POST_BUILD
  COMMAND mt.exe -manifest ${CMAKE_CURRENT_SOURCE_DIR}/upgrade_wizard.exe.manifest
  "-outputresource:${upgrade_wizard_location};#1"
)
