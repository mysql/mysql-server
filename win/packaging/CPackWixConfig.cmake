
IF(ESSENTIALS)
 SET(CPACK_COMPONENTS_USED "Server;Client")
 SET(CPACK_WIX_UI "MyWixUI_Mondo")
 IF(CMAKE_SIZEOF_VOID_P MATCHES 8)
  SET(CPACK_PACKAGE_FILE_NAME  "mariadb-essential-${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}-winx64")
 ELSE()
  SET(CPACK_PACKAGE_FILE_NAME  "mariadb-essential-${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}-win32")
 ENDIF()
ELSE()
  SET(CPACK_COMPONENTS_USED 
    "Server;Client;Development;SharedLibraries;Embedded;Documentation;IniFiles;Readme;Debuginfo;Common")
ENDIF()

SET( WIX_FEATURE_MySQLServer_EXTRA_FEATURES "DBInstance;SharedClientServerComponents")
# Some components like Embedded are optional
# We will build MSI without embedded if it was not selected for build
#(need to modify CPACK_COMPONENTS_ALL for that)
SET(CPACK_ALL)
FOREACH(comp1 ${CPACK_COMPONENTS_USED})
 SET(found)
 FOREACH(comp2 ${CPACK_COMPONENTS_ALL})
  IF(comp1 STREQUAL comp2)
    SET(found 1)
    BREAK()
  ENDIF()
 ENDFOREACH()
 IF(found)
   SET(CPACK_ALL ${CPACK_ALL} ${comp1})
 ENDIF()
ENDFOREACH()
SET(CPACK_COMPONENTS_ALL ${CPACK_ALL})

# Always install (hidden), includes Readme files
SET(CPACK_COMPONENT_GROUP_ALWAYSINSTALL_HIDDEN 1)
SET(CPACK_COMPONENT_README_GROUP "AlwaysInstall")
SET(CPACK_COMPONENT_COMMON_GROUP "AlwaysInstall")

# Feature MySQL Server
SET(CPACK_COMPONENT_GROUP_MYSQLSERVER_DISPLAY_NAME "MariaDB Server")
SET(CPACK_COMPONENT_GROUP_MYSQLSERVER_EXPANDED "1")
SET(CPACK_COMPONENT_GROUP_MYSQLSERVER_DESCRIPTION "Install server")
 # Subfeature "Server" (hidden)
 SET(CPACK_COMPONENT_SERVER_GROUP "MySQLServer")
 SET(CPACK_COMPONENT_SERVER_HIDDEN 1)
 # Subfeature "Client" 
 SET(CPACK_COMPONENT_CLIENT_GROUP "MySQLServer")
 SET(CPACK_COMPONENT_CLIENT_DISPLAY_NAME "Client Programs")
 SET(CPACK_COMPONENT_CLIENT_DESCRIPTION 
   "Various helpful (commandline) tools including the mysql command line client" )
 # Subfeature "Debug binaries" 
 SET(CPACK_COMPONENT_DEBUGBINARIES_GROUP "MySQLServer")
 SET(CPACK_COMPONENT_DEBUGBINARIES_DISPLAY_NAME "Debug binaries")
 SET(CPACK_COMPONENT_DEBUGBINARIES_DESCRIPTION 
   "Debug/trace versions of executables and libraries" )
 #SET(CPACK_COMPONENT_DEBUGBINARIES_WIX_LEVEL 2)
  
   
 #Subfeature "Data Files" 
 SET(CPACK_COMPONENT_DATAFILES_GROUP "MySQLServer")
 SET(CPACK_COMPONENT_DATAFILES_DISPLAY_NAME "Server data files")
 SET(CPACK_COMPONENT_DATAFILES_DESCRIPTION "Server data files" )
 SET(CPACK_COMPONENT_DATAFILES_HIDDEN 1)
 
 
#Feature "Devel"
SET(CPACK_COMPONENT_GROUP_DEVEL_DISPLAY_NAME "Development Components")
SET(CPACK_COMPONENT_GROUP_DEVEL_DESCRIPTION "Installs C/C++ header files and libraries")
 #Subfeature "Development"
 SET(CPACK_COMPONENT_DEVELOPMENT_GROUP "Devel")
 SET(CPACK_COMPONENT_DEVELOPMENT_HIDDEN 1)
 
 #Subfeature "Shared libraries"
 SET(CPACK_COMPONENT_SHAREDLIBRARIES_GROUP "Devel")
 SET(CPACK_COMPONENT_SHAREDLIBRARIES_DISPLAY_NAME "Client C API library (shared)")
 SET(CPACK_COMPONENT_SHAREDLIBRARIES_DESCRIPTION "Installs shared client library")
 
 #Subfeature "Embedded"
 SET(CPACK_COMPONENT_EMBEDDED_GROUP "Devel")
 SET(CPACK_COMPONENT_EMBEDDED_DISPLAY_NAME "Embedded server library")
 SET(CPACK_COMPONENT_EMBEDDED_DESCRIPTION "Installs embedded server library")
 SET(CPACK_COMPONENT_EMBEDDED_WIX_LEVEL 2)

#Feature Debug Symbols
SET(CPACK_COMPONENT_GROUP_DEBUGSYMBOLS_DISPLAY_NAME "Debug Symbols")
SET(CPACK_COMPONENT_GROUP_DEBUGSYMBOLS_DESCRIPTION "Installs Debug Symbols")
SET(CPACK_COMPONENT_DEBUGSYMBOLS_WIX_LEVEL 2)
 SET(CPACK_COMPONENT_DEBUGINFO_GROUP "DebugSymbols")
 SET(CPACK_COMPONENT_DEBUGINFO_HIDDEN 1)

#Feature Documentation
SET(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")
SET(CPACK_COMPONENT_DOCUMENTATION_DESCRIPTION "Installs documentation")
SET(CPACK_COMPONENT_DOCUMENTATION_WIX_LEVEL 2)

#Feature tests
SET(CPACK_COMPONENT_TEST_DISPLAY_NAME "Tests")
SET(CPACK_COMPONENT_TEST_DESCRIPTION "Installs unittests (requires Perl to run)")
SET(CPACK_COMPONENT_TEST_WIX_LEVEL 2)


#Feature Misc (hidden, installs only if everything is installed)
SET(CPACK_COMPONENT_GROUP_MISC_HIDDEN 1)
SET(CPACK_COMPONENT_GROUP_MISC_WIX_LEVEL 100)
  SET(CPACK_COMPONENT_INIFILES_GROUP "Misc")
  SET(CPACK_COMPONENT_SERVER_SCRIPTS_GROUP "Misc")

#Add Firewall exception for mysqld.exe
SET(bin.mysqld.exe.FILE_EXTRA "
  <FirewallException Id='firewallexception.mysqld.exe' Name='[ProductName]' Scope='any'
       IgnoreFailure='yes' xmlns='http://schemas.microsoft.com/wix/FirewallExtension' 
  />
  "
)

