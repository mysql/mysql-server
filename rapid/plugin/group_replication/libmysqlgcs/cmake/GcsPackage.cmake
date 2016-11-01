# Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

# Set versions for package name
SET(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
SET(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
SET(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})

SET(XCOM_BINARY_DIR ${PROJECT_BINARY_DIR}/src/bindings/xcom/xcom)
SET(XCOM_SUBTREE_LOCATION ${PROJECT_SOURCE_DIR}/src/bindings/xcom/xcom)

# Set XCom static and shared library paths
IF(WIN32)
  SET(XCOM_STATIC_LIBRARY ${XCOM_BINARY_DIR}/${CMAKE_BUILD_TYPE}/xcom.lib)
  SET(XCOM_SHARED_LIBRARY
      ${XCOM_BINARY_DIR}/${CMAKE_BUILD_TYPE}/xcom_shared.dll)
ELSE()
  SET(XCOM_STATIC_LIBRARY ${XCOM_BINARY_DIR}/libxcom.a)
  IF(APPLE)
    SET(XCOM_SHARED_LIBRARY ${XCOM_BINARY_DIR}/libxcom_shared.dylib)
  ELSE()
    SET(XCOM_SHARED_LIBRARY ${XCOM_BINARY_DIR}/libxcom_shared.so)
  ENDIF()
ENDIF()

# Installing XCom shared library
INSTALL(FILES ${XCOM_SHARED_LIBRARY}
        DESTINATION ${INSTALL_LIB}
        COMPONENT ${COMP_BIN})

# Installing XCom static library
INSTALL(FILES ${XCOM_STATIC_LIBRARY}
        DESTINATION ${INSTALL_LIB}
        COMPONENT ${COMP_BIN})

# install LICENSE, VERSION and README
INSTALL(FILES
          LICENSE
          VERSION
          README
        DESTINATION .
        COMPONENT ${COMP_BIN})

IF(SPHINX_BUILD_DIR)

  INSTALL(DIRECTORY ${SPHINX_BUILD_DIR}
          DESTINATION ${INSTALL_DOCS}/tutorial
          COMPONENT ${COMP_BIN})

  INSTALL(DIRECTORY ${SPHINX_BUILD_DIR}
          DESTINATION ${INSTALL_DOCS}/tutorial
          COMPONENT ${COMP_DOCS})
ENDIF()

IF(DOXYGEN_OUTPUT_DIR)
  INSTALL(DIRECTORY ${DOXYGEN_OUTPUT_DIR}/html
          DESTINATION ${INSTALL_DOCS}/api
          COMPONENT ${COMP_BIN})

  INSTALL(DIRECTORY ${DOXYGEN_OUTPUT_DIR}/html
          DESTINATION ${INSTALL_DOCS}/api
          COMPONENT ${COMP_DOCS})
ENDIF()

# install header files
INSTALL(DIRECTORY include
        DESTINATION .
        COMPONENT ${COMP_BIN})

##
## COMP_SRC
##
INSTALL(FILES
          LICENSE
          VERSION
          README
          mysql_gcs.h.cmake
          CMakeLists.txt
        DESTINATION .
        COMPONENT ${COMP_SRC})

INSTALL(DIRECTORY include
        DESTINATION .
        COMPONENT ${COMP_SRC})

INSTALL(DIRECTORY src
        DESTINATION .
        COMPONENT ${COMP_SRC})

INSTALL(DIRECTORY docs
        DESTINATION .
        COMPONENT ${COMP_SRC})

INSTALL(DIRECTORY cmake
        DESTINATION .
        COMPONENT ${COMP_SRC})


# Other install directives are in sub CMakeLists.txt

##
## COMP_SRC_GR
##

# extra for libmysqlgcs component
INSTALL(FILES
          LICENSE
          VERSION
          README
          mysql_gcs.h.cmake
          CMakeLists.txt
        DESTINATION .
        COMPONENT ${COMP_SRC_GR})

INSTALL(DIRECTORY include
        DESTINATION .
        COMPONENT ${COMP_SRC_GR})

INSTALL(DIRECTORY src
        DESTINATION .
        COMPONENT ${COMP_SRC_GR}
        PATTERN "tests" EXCLUDE
        PATTERN "examples" EXCLUDE
        PATTERN "wrappers" EXCLUDE
        PATTERN "extra/lz4" EXCLUDE
        PATTERN "security" EXCLUDE
        PATTERN "bindings/xcom/xcom/examples" EXCLUDE
        PATTERN "bindings/xcom/xcom/yassl" EXCLUDE
        PATTERN "bindings/xcom/xcom/enum.awk" EXCLUDE
        PATTERN "bindings/xcom/xcom/run-cmake.sh" EXCLUDE)

INSTALL(DIRECTORY cmake
        DESTINATION .
        COMPONENT ${COMP_SRC_GR}
        PATTERN "FindSphinx.cmake" EXCLUDE
        PATTERN "GcsDocs.cmake" EXCLUDE
        PATTERN "gmock.cmake" EXCLUDE)

# Set the components
SET(COMPONENTS ${COMP_BIN} ${COMP_SRC} ${COMP_TESTS} ${COMP_DOCS})
IF (NOT CPACK_COMPONENTS_ALL)
  SET(CPACK_COMPONENTS_ALL ${COMPONENTS})
ENDIF()

SET(CPACK_COMPONENT_INCLUDE_TOPLEVEL_DIRECTORY ON)
SET(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
IF(CMAKE_BUILD_TYPE)
  SET(CPACK_PACKAGE_FILE_NAME
      ${PROJECT_NAME}-${PROJECT_VERSION}-${CMAKE_BUILD_TYPE})
ENDIF()
SET(CPACK_TOPLEVEL_TAG ${CPACK_PACKAGE_FILE_NAME})
SET(CPACK_PACKAGE_VENDOR "Oracle")
SET(CPACK_GENERATOR "ZIP")
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README")
SET(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")

SET(CPACK_ARCHIVE_COMPONENT_INSTALL ON)

IF(WIN32)
  EXECUTE_PROCESS(COMMAND subst.exe w: /D)
  EXECUTE_PROCESS(COMMAND subst.exe w: .)
  SET(CPACK_PACKAGE_DIRECTORY w:)
ENDIF()

INCLUDE(CPack)
