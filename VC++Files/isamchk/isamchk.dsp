# Microsoft Developer Studio Project File - Name="isamchk" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=isamchk - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "isamchk.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "isamchk.mak" CFG="isamchk - Win32 Debug"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "isamchk - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "isamchk - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "isamchk - Win32 classic" (based on "Win32 (x86) Console Application")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl6.exe
RSC=rc.exe

!IF  "$(CFG)" == "isamchk - Win32 Release"

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
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../isam" /D "DBUG_OFF" /D "_CONSOLE" /D "_MBCS" /D "_WINDOWS" /D "NDEBUG" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x41d /d "NDEBUG"
# ADD RSC /l 0x41d /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib setargv.obj /nologo /subsystem:console /machine:I386 /out:"../client_release/isamchk.exe"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "isamchk - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "isamchk_"
# PROP BASE Intermediate_Dir "isamchk_"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G6 /MTd /W3 /Z7 /Od /I "../include" /I "../isam" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_CONSOLE" /D "_MBCS" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /Fr /YX
# ADD BASE RSC /l 0x41d /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib wsock32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib setargv.obj /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"../client_debug/isamchk.exe" /pdbtype:sept
# SUBTRACT LINK32 /verbose /pdb:none

!ELSEIF  "$(CFG)" == "isamchk - Win32 classic"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "isamchk___Win32_classic"
# PROP BASE Intermediate_Dir "isamchk___Win32_classic"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "classic"
# PROP Intermediate_Dir "classic"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../isam" /D "DBUG_OFF" /D "_CONSOLE" /D "_MBCS" /D "_WINDOWS" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../isam" /D "_CONSOLE" /D "_WINDOWS" /D "DBUG_OFF" /D "_MBCS" /D "NDEBUG" /D LICENSE=Commercial /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x41d /d "NDEBUG"
# ADD RSC /l 0x41d /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib setargv.obj /nologo /subsystem:console /machine:I386 /out:"../client_release/isamchk.exe"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib setargv.obj ..\lib_release\isam.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib /nologo /subsystem:console /machine:I386 /out:"../client_classic/isamchk.exe" /libpath:"..\lib_release\\"
# SUBTRACT LINK32 /pdb:none

!ENDIF

# Begin Target

# Name "isamchk - Win32 Release"
# Name "isamchk - Win32 Debug"
# Name "isamchk - Win32 classic"
# Begin Source File

SOURCE=..\isam\isamchk.c
# End Source File
# Begin Source File

SOURCE=..\isam\sort.c
# End Source File
# End Target
# End Project
