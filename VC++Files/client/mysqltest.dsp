# Microsoft Developer Studio Project File - Name="mysqltest" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=mysqltest - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mysqltest.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mysqltest.mak" CFG="mysqltest - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mysqltest - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqltest - Win32 classic" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqltest - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mysqltest - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\debug"
# PROP BASE Intermediate_Dir ".\debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\debug"
# PROP Intermediate_Dir ".\debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /I "../include" /I "../" /Z7 /W3 /Od /G6 /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_CONSOLE" /D "_WINDOWS" /D "USE_TLS" /D "_MBCS" /Fp".\debug/mysqltest.pch" /Fo".\debug/" /Fd".\debug/" /GZ /c /GX 
# ADD CPP /nologo /MTd /I "../include" /I "../" /Z7 /W3 /Od /G6 /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_CONSOLE" /D "_WINDOWS" /D "USE_TLS" /D "_MBCS" /Fp".\debug/mysqltest.pch" /Fo".\debug/" /Fd".\debug/" /GZ /c /GX 
# ADD BASE MTL /nologo /tlb".\debug\mysqltest.tlb" /win32 
# ADD MTL /nologo /tlb".\debug\mysqltest.tlb" /win32 
# ADD BASE RSC /l 1033 /d "_DEBUG" 
# ADD RSC /l 1033 /d "_DEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo 
# ADD BSC32 /nologo 
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysqlclient.lib wsock32.lib odbc32.lib odbccp32.lib mysys.lib /nologo /out:"..\client_debug\mysqltest.exe" /incremental:no /libpath:"..\lib_debug\" /debug /pdb:".\debug\mysqltest.pdb" /pdbtype:sept /subsystem:console /MACHINE:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysqlclient.lib wsock32.lib odbc32.lib odbccp32.lib mysys.lib /nologo /out:"..\client_debug\mysqltest.exe" /incremental:no /libpath:"..\lib_debug\" /debug /pdb:".\debug\mysqltest.pdb" /pdbtype:sept /subsystem:console /MACHINE:I386

!ELSEIF  "$(CFG)" == "mysqltest - Win32 classic"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\classic"
# PROP BASE Intermediate_Dir ".\classic"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\classic"
# PROP Intermediate_Dir ".\classic"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /I "../include" /I "../" /W3 /Ob1 /G6 /D "_CONSOLE" /D "_WINDOWS" /D "LICENSE=Commercial" /D "DBUG_OFF" /D "NDEBUG" /D "_MBCS" /GF /Gy /Fp".\classic/mysqltest.pch" /Fo".\classic/" /Fd".\classic/" /c /GX 
# ADD CPP /nologo /MT /I "../include" /I "../" /W3 /Ob1 /G6 /D "_CONSOLE" /D "_WINDOWS" /D "LICENSE=Commercial" /D "DBUG_OFF" /D "NDEBUG" /D "_MBCS" /GF /Gy /Fp".\classic/mysqltest.pch" /Fo".\classic/" /Fd".\classic/" /c /GX 
# ADD BASE MTL /nologo /tlb".\classic\mysqltest.tlb" /win32 
# ADD MTL /nologo /tlb".\classic\mysqltest.tlb" /win32 
# ADD BASE RSC /l 1033 /d "NDEBUG" 
# ADD RSC /l 1033 /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo 
# ADD BSC32 /nologo 
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysqlclient.lib wsock32.lib odbc32.lib odbccp32.lib /nologo /out:"..\client_classic\mysqltest.exe" /incremental:no /libpath:"..\lib_release\" /pdb:".\classic\mysqltest.pdb" /pdbtype:sept /subsystem:console /MACHINE:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysqlclient.lib wsock32.lib odbc32.lib odbccp32.lib /nologo /out:"..\client_classic\mysqltest.exe" /incremental:no /libpath:"..\lib_release\" /pdb:".\classic\mysqltest.pdb" /pdbtype:sept /subsystem:console /MACHINE:I386

!ELSEIF  "$(CFG)" == "mysqltest - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\release"
# PROP BASE Intermediate_Dir ".\release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\release"
# PROP Intermediate_Dir ".\release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /I "../include" /I "../" /W3 /Ob1 /G6 /D "DBUG_OFF" /D "_CONSOLE" /D "_WINDOWS" /D "NDEBUG" /D "_MBCS" /GF /Gy /Fp".\release/mysqltest.pch" /Fo".\release/" /Fd".\release/" /c /GX 
# ADD CPP /nologo /MT /I "../include" /I "../" /W3 /Ob1 /G6 /D "DBUG_OFF" /D "_CONSOLE" /D "_WINDOWS" /D "NDEBUG" /D "_MBCS" /GF /Gy /Fp".\release/mysqltest.pch" /Fo".\release/" /Fd".\release/" /c /GX 
# ADD BASE MTL /nologo /tlb".\release\mysqltest.tlb" /win32 
# ADD MTL /nologo /tlb".\release\mysqltest.tlb" /win32 
# ADD BASE RSC /l 1033 /d "NDEBUG" 
# ADD RSC /l 1033 /d "NDEBUG" 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo 
# ADD BSC32 /nologo 
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysqlclient.lib wsock32.lib odbc32.lib odbccp32.lib /nologo /out:"..\client_release\mysqltest.exe" /incremental:no /libpath:"..\lib_release\" /pdb:".\release\mysqltest.pdb" /pdbtype:sept /subsystem:console /MACHINE:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysqlclient.lib wsock32.lib odbc32.lib odbccp32.lib /nologo /out:"..\client_release\mysqltest.exe" /incremental:no /libpath:"..\lib_release\" /pdb:".\release\mysqltest.pdb" /pdbtype:sept /subsystem:console /MACHINE:I386

!ENDIF

# Begin Target

# Name "mysqltest - Win32 Debug"
# Name "mysqltest - Win32 classic"
# Name "mysqltest - Win32 Release"
# Begin Source File

SOURCE=..\libmysql\manager.c
# End Source File
# Begin Source File

SOURCE=.\mysqltest.c
# End Source File
# End Target
# End Project

