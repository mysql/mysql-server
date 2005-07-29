# Microsoft Developer Studio Project File - Name="mysql" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=mysql - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "mysql.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "mysql.mak" CFG="mysql - Win32 Debug"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "mysql - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "mysql - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "mysql - Win32 classic" (based on "Win32 (x86) Console Application")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl6.exe
RSC=rc.exe

!IF  "$(CFG)" == "mysql - Win32 Release"

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
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../" /D "NDEBUG" /D "DBUG_OFF" /D "_CONSOLE" /D "_MBCS" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /WX /Fr /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 mysqlclient.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib zlib.lib /nologo /subsystem:console /debug /machine:I386 /out:"../client_release/mysql.exe" /libpath:"..\lib_release\\"
# SUBTRACT LINK32 /incremental:yes

!ELSEIF  "$(CFG)" == "mysql - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "mysql___"
# PROP BASE Intermediate_Dir "mysql___"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G6 /MTd /W3 /Z7 /Od /I "../include" /I "../" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_CONSOLE" /D "_MBCS" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /Fr /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 mysqlclient.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib zlib.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"../client_debug/mysql.exe" /pdbtype:sept /libpath:"..\lib_debug\\"

!ELSEIF  "$(CFG)" == "mysql - Win32 classic"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysql___Win32_classic"
# PROP BASE Intermediate_Dir "mysql___Win32_classic"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "classic"
# PROP Intermediate_Dir "classic"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /WX /O2 /I "../include" /I "../" /D "DBUG_OFF" /D "_CONSOLE" /D "_MBCS" /D "_WINDOWS" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /G6 /MT /W3 /WX /O2 /I "../include" /I "../" /D "_CONSOLE" /D "_WINDOWS" /D LICENSE=Commercial /D "DBUG_OFF" /D "_MBCS" /D "NDEBUG" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 mysqlclient.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /out:"../client_release/mysql.exe" /libpath:"..\lib_release\\"
# SUBTRACT BASE LINK32 /incremental:yes
# ADD LINK32 mysqlclient.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib zlib.lib /nologo /subsystem:console /debug /machine:I386 /out:"../client_classic/mysql.exe" /libpath:"..\lib_release\\"
# SUBTRACT LINK32 /incremental:yes

!ENDIF

# Begin Target

# Name "mysql - Win32 Release"
# Name "mysql - Win32 Debug"
# Name "mysql - Win32 classic"
# Begin Source File

SOURCE=.\completion_hash.cpp
# End Source File
# Begin Source File

SOURCE=.\mysql.cpp

!IF  "$(CFG)" == "mysql - Win32 Release"

# ADD CPP /Zi /O2

!ELSEIF  "$(CFG)" == "mysql - Win32 Debug"

!ELSEIF  "$(CFG)" == "mysql - Win32 classic"

# ADD BASE CPP /Zi /O2
# ADD CPP /Zi /O2

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\readline.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_string.cpp
# End Source File
# End Target
# End Project
