# Microsoft Developer Studio Project File - Name="mysqlserver" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=mysqlserver - WinIA64 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mysqlserver.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mysqlserver.mak" CFG="mysqlserver - WinIA64 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mysqlserver - WinIA64 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "mysqlserver - WinIA64 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mysqlserver - WinIA64 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "release"
# PROP Intermediate_Dir "release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN64" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../regex" /I "../sql" /I "../bdb/build_win64" /I "../libmysqld" /D "WIN64" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "HAVE_BERKELEY_DB" /D "SIGNAL_WITH_VIO_CLOSE" /D "HAVE_DLOPEN" /D "EMBEDDED_LIBRARY" /D "HAVE_INNOBASE_DB" /D "DBUG_OFF" /D "USE_TLS" /D "_IA64_" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /YX /FD /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x416 /d "NDEBUG"
# ADD RSC /l 0x416 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "mysqlserver - WinIA64 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN64" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Zi /Od /I "../include" /I "../regex" /I "../sql" /I "../bdb/build_win64" /I "libmysqld" /D "WIN64" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "HAVE_BERKELEY_DB" /D "USE_SYMDIR" /D "SIGNAL_WITH_VIO_CLOSE" /D "HAVE_DLOPEN" /D "EMBEDDED_LIBRARY" /D "HAVE_INNOBASE_DB" /D "USE_TLS" /D "_IA64_" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /YX /FD /GZ /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x416 /d "_DEBUG"
# ADD RSC /l 0x416 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "mysqlserver - WinIA64 Release"
# Name "mysqlserver - WinIA64 Debug"
# End Target
# End Project
