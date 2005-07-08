# Microsoft Developer Studio Project File - Name="mysql_client_test" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=mysql_client_test - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mysql_client_test.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mysql_client_test.mak" CFG="mysql_client_test - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mysql_client_test - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "mysql_client_test - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mysql_client_test - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\Debug"
# PROP BASE Intermediate_Dir ".\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\Debug"
# PROP Intermediate_Dir ".\Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /I "../include" /I "../" /Z7 /W3 /Od /G6 /D "_DEBUG" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN32" /Fp".\Debug/mysql_client_test.pch" /Fo".\Debug/" /Fd".\Debug/" /GZ /FD /c /GX 
# ADD CPP /nologo /MTd /I "../include" /I "../" /Z7 /W3 /Od /G6 /D "_DEBUG" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN32" /Fp".\Debug/mysql_client_test.pch" /Fo".\Debug/" /Fd".\Debug/" /GZ /FD /c /GX 
# ADD BASE MTL /nologo /tlb".\Debug\mysql_client_test.tlb" /win32 
# ADD MTL /nologo /tlb".\Debug\mysql_client_test.tlb" /win32 
# ADD BASE RSC /l 1033 
# ADD RSC /l 1033 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo 
# ADD BSC32 /nologo 
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib odbc32.lib odbccp32.lib mysqlclient.lib wsock32.lib mysys.lib regex.lib /nologo /out:"..\client_debug\mysql_client_test.exe" /incremental:yes /libpath:"..\lib_debug\" /debug /pdb:".\Debug\mysql_client_test.pdb" /pdbtype:sept /map:".\Debug\mysql_client_test.map" /subsystem:console 
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib odbc32.lib odbccp32.lib mysqlclient.lib wsock32.lib mysys.lib regex.lib /nologo /out:"..\client_debug\mysql_client_test.exe" /incremental:yes /libpath:"..\lib_debug\" /debug /pdb:".\Debug\mysql_client_test.pdb" /pdbtype:sept /map:".\Debug\mysql_client_test.map" /subsystem:console 

!ELSEIF  "$(CFG)" == "mysql_client_test - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\Release"
# PROP BASE Intermediate_Dir ".\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\Release"
# PROP Intermediate_Dir ".\Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /I "../include" /I "../" /W3 /Ob1 /G6 /D "DBUG_OFF" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN32" /GF /Gy /Fp".\Release/mysql_client_test.pch" /Fo".\Release/" /Fd".\Release/" /FD /c /GX 
# ADD CPP /nologo /MTd /I "../include" /I "../" /W3 /Ob1 /G6 /D "DBUG_OFF" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN32" /GF /Gy /Fp".\Release/mysql_client_test.pch" /Fo".\Release/" /Fd".\Release/" /FD /c /GX 
# ADD BASE MTL /nologo /tlb".\Release\mysql_client_test.tlb" /win32 
# ADD MTL /nologo /tlb".\Release\mysql_client_test.tlb" /win32 
# ADD BASE RSC /l 1033 
# ADD RSC /l 1033 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo 
# ADD BSC32 /nologo 
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib odbc32.lib odbccp32.lib Ws2_32.lib mysqlclient.lib mysys.lib regex.lib /nologo /out:"..\client_release\mysql_client_test.exe" /incremental:no /libpath:"..\lib_release\" /pdb:".\Release\mysql_client_test.pdb" /pdbtype:sept /subsystem:console 
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib odbc32.lib odbccp32.lib Ws2_32.lib mysqlclient.lib mysys.lib regex.lib /nologo /out:"..\client_release\mysql_client_test.exe" /incremental:no /libpath:"..\lib_release\" /pdb:".\Release\mysql_client_test.pdb" /pdbtype:sept /subsystem:console 

!ENDIF

# Begin Target

# Name "mysql_client_test - Win32 Debug"
# Name "mysql_client_test - Win32 Release"
# Begin Source File

SOURCE=mysql_client_test.c
# End Source File
# End Target
# End Project

