# Microsoft Developer Studio Project File - Name="mysqldump" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=mysqldump - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "mysqldump.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "mysqldump.mak" CFG="mysqldump - Win32 Debug"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "mysqldump - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqldump - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqldump - Win32 classic" (based on "Win32 (x86) Console Application")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl6.exe
RSC=rc.exe

!IF  "$(CFG)" == "mysqldump - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../" /D "DBUG_OFF" /D "_CONSOLE" /D "_MBCS" /D "_WINDOWS" /D "NDEBUG" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysys.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 mysqlclient.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysys.lib zlib.lib ..\extra\yassl\Release\yassl.lib /nologo /subsystem:console /machine:I386 /out:"../client_release/mysqldump.exe" /libpath:"..\lib_release\\"

!ELSEIF  "$(CFG)" == "mysqldump - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G6 /MTd /W3 /Z7 /Od /I "../include" /I "../" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_CONSOLE" /D "_MBCS" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysys.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 mysqlclient.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysys.lib zlib.lib ..\extra\yassl\Debug\yassl.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"../client_debug/mysqldump.exe" /pdbtype:sept /libpath:"..\lib_debug\\"

!ELSEIF  "$(CFG)" == "mysqldump - Win32 classic"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqldump___Win32_classic"
# PROP BASE Intermediate_Dir "mysqldump___Win32_classic"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "classic"
# PROP Intermediate_Dir "classic"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../" /D "DBUG_OFF" /D "_CONSOLE" /D "_MBCS" /D "_WINDOWS" /D "NDEBUG"  /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../" /D "_CONSOLE" /D "_WINDOWS" /D LICENSE=Commercial /D "DBUG_OFF" /D "_MBCS" /D "NDEBUG" /FD  /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 mysqlclient.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysys.lib /nologo /subsystem:console /machine:I386 /out:"../client_release/mysqldump.exe" /libpath:"..\lib_release\\"
# ADD LINK32 mysqlclient.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysys.lib zlib.lib ..\extra\yassl\Release\yassl.lib /nologo /subsystem:console /machine:I386 /out:"../client_classic/mysqldump.exe" /libpath:"..\lib_release\\"

!ENDIF

# Begin Target

# Name "mysqldump - Win32 Release"
# Name "mysqldump - Win32 Debug"
# Name "mysqldump - Win32 classic"
# Begin Source File

SOURCE=.\mysqldump.c

!IF  "$(CFG)" == "mysqldump - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldump - Win32 Debug"

# ADD CPP /W3
# SUBTRACT CPP /YX

!ELSEIF  "$(CFG)" == "mysqldump - Win32 classic"

!ENDIF

# End Source File
# End Target
# End Project
