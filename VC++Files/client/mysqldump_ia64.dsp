# Microsoft Developer Studio Project File - Name="mysqldump" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=mysqldump - WinIA64 classic
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mysqldump_ia64.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mysqldump_ia64.mak" CFG="mysqldump - WinIA64 classic"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mysqldump - WinIA64 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqldump - WinIA64 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqldump - WinIA64 classic" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mysqldump - WinIA64 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "release"
# PROP Intermediate_Dir "release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN64" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../" /D "DBUG_OFF" /D "_CONSOLE" /D "_MBCS" /D "_WINDOWS" /D "NDEBUG" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysys.lib /nologo /subsystem:console  /machine:IA64
# ADD LINK32 ..\lib_release\zlib.lib mysqlclient.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysys.lib bufferoverflowU.lib /nologo /subsystem:console  /out:"../client_release/mysqldump.exe" /libpath:"..\lib_release\\" /machine:IA64

!ELSEIF  "$(CFG)" == "mysqldump - WinIA64 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "mysqldum"
# PROP BASE Intermediate_Dir "mysqldum"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN64" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Zi /Od /I "../include" /I "../" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_CONSOLE" /D "_MBCS" /D "_WINDOWS" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysys.lib /nologo /subsystem:console /debug  /machine:IA64
# ADD LINK32 ..\lib_debug\dbug.lib ..\lib_debug\zlib.lib mysqlclient.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysys.lib bufferoverflowU.lib /nologo /subsystem:console /incremental:no /debug  /out:"../client_debug/mysqldump.exe" /libpath:"..\lib_debug\\" /machine:IA64

!ELSEIF  "$(CFG)" == "mysqldump - WinIA64 classic"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqldump___WinIA64_classic"
# PROP BASE Intermediate_Dir "mysqldump___WinIA64_classic"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "classic"
# PROP Intermediate_Dir "classic"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../" /D "DBUG_OFF" /D "_CONSOLE" /D "_MBCS" /D "_WINDOWS" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../" /D "_CONSOLE" /D "_WINDOWS" /D LICENSE=Commercial /D "DBUG_OFF" /D "_MBCS" /D "NDEBUG" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 mysqlclient.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysys.lib /nologo /subsystem:console  /out:"../client_release/mysqldump.exe" /libpath:"..\lib_release\\" /machine:IA64
# ADD LINK32 ..\lib_release\zlib.lib mysqlclient.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysys.lib bufferoverflowU.lib /nologo /subsystem:console  /out:"../client_classic/mysqldump.exe" /libpath:"..\lib_release\\" /machine:IA64

!ENDIF 

# Begin Target

# Name "mysqldump - WinIA64 Release"
# Name "mysqldump - WinIA64 Debug"
# Name "mysqldump - WinIA64 classic"
# Begin Source File

SOURCE=.\mysqldump.c

!IF  "$(CFG)" == "mysqldump - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqldump - WinIA64 Debug"

# ADD CPP /W3 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX

!ELSEIF  "$(CFG)" == "mysqldump - WinIA64 classic"

!ENDIF 

# End Source File
# End Target
# End Project
