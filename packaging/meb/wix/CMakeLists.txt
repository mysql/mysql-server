# Copyright (c) 2015, 2017, Oracle and/or its affiliates. All Rights Reversed.

#
# This program is NOT released under the GPL license, but constitutes
# a trade secret of Oracle. Use, publication, and redistribution
# of this program is prohibited without a written permission from
# Oracle.

##############################################################################
# Set the platform name and where to install
##############################################################################

# This is Windows only, so just set one of two possible

set(PLATFORM_WIX "x86")
set(MEB_DEFAULT_DEST "ProgramFilesFolder")
if(CMAKE_SIZEOF_VOID_P MATCHES 8)
  set(PLATFORM_WIX "x64")
  set(MEB_DEFAULT_DEST "ProgramFiles64Folder")
endif()


##############################################################################
# Find the WiX directory where light and candle are located
##############################################################################

find_path(WIX_BIN_DIR candle.exe
        $ENV{WIX_DIR}
        $ENV{WIX_DIR}/bin
        $ENV{ProgramFiles}/wix/bin
        $ENV{ProgramFiles}/Windows Installer */bin)

if(NOT WIX_BIN_DIR)
  message(ERROR "Wix not found. Please change your environment variable PATH or specify WIX_DIR")
endif()

##############################################################################
#  Set the output package name
##############################################################################

if(NOT EXTRA_NAME_SUFFIX)
  set(EXTRA_NAME_SUFFIX "")
endif()

set(WIXOUT "meb${EXTRA_NAME_SUFFIX}-${MEB_PACKAGE_VERSION}-${MEB_PLATFORM}")
message(STATUS "Output package name: ${WIXOUT}")

# set(ProductURN "MSQZP-100-ZZZZ")

#-----------------------------------------------------

set(LICENSE_FILE "${MEB_LICENSE_FILE}")
set(LICENSE_RTF  "${CMAKE_CURRENT_BINARY_DIR}/EULA.rtf")
file(READ ${LICENSE_FILE} CONTENTS)
string(REGEX REPLACE "\n" "\\\\par\n" CONTENTS "${CONTENTS}")
string(REGEX REPLACE "\t" "\\\\tab" CONTENTS "${CONTENTS}")
file(WRITE "${LICENSE_RTF}"
     "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fnil\\fcharset0 Courier New;}}\\viewkind4\\uc1\\pard\\lang1031\\f0\\fs15")
file(APPEND "${LICENSE_RTF}" "${CONTENTS}")
file(APPEND "${LICENSE_RTF}" "\n}\n")

#-----------------------------------------------------

set(GEN_WXS "${CMAKE_CURRENT_BINARY_DIR}/meb.wxs")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/meb.wxs.in ${GEN_WXS} @ONLY)
set_source_files_properties(${GEN_WXS} PROPERTIES GENERATED 1)

set(WIX_SOURCE
    MEB_BrowseDlg.wxs
    MEB_CancelDlg.wxs
    MEB_DiskCostDlg.wxs
    MEB_ExitDialog.wxs
    MEB_InstallDirDlg.wxs
    MEB_InstallDirUI.wxs
    MEB_LicenseAgreementDlg.wxs
    MEB_MaintenanceTypeDlg.wxs
    MEB_MaintenanceWelcomeDlg.wxs
    MEB_ProgressDlg.wxs
    MEB_ResumeDlg.wxs
    MEB_UserExit.wxs
    MEB_VerifyReadyDlg.wxs
    MEB_WelcomeDlg.wxs
    ${GEN_WXS}
)

set(MEB_LOCALIZATION "MEB_en-us.wxl")
set(WIX_MSI "${CMAKE_BINARY_DIR}/${WIXOUT}.msi")
set(WIX_OBJECTS)

# Create a list of intermediate object files
foreach(_wxl ${WIX_SOURCE})
  get_filename_component(_name ${_wxl} NAME_WE)
  list(APPEND WIX_OBJECTS "${CMAKE_CURRENT_BINARY_DIR}/${_name}.wixobj")
endforeach()

add_custom_command(OUTPUT ${WIX_OBJECTS}
                   COMMAND "${WIX_BIN_DIR}/candle"
		     "-dBinaryDir=${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}"
		     "-dDocDir=${CMAKE_SOURCE_DIR}/../docs"
		     -arch ${PLATFORM_WIX}
		     -o "${CMAKE_CURRENT_BINARY_DIR}/"
                     ${WIX_SOURCE}
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_custom_target(msi
                   COMMAND "${WIX_BIN_DIR}/light"
		     "-sice:ICE01;ICE02;ICE03;ICE04;ICE05;ICE06;ICE07"
		     -cultures:en-us
		     -ext WixUIExtension
		     -loc ${MEB_LOCALIZATION}
                     -o "${WIX_MSI}"
                     ${WIX_OBJECTS}
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                   DEPENDS ${WIX_OBJECTS})
