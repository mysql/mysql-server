# Microsoft Developer Studio Project File - Name="mysql_client_test" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win3 (x86) Console Application" 0x0103

CFG=mysql_client_test - WinIA64 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mysql_client_test.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mysql_client_test.mak" CFG="mysql_client_test - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mysql_client_test - WinIA64 Release" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe
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
MTL=midl.exe
# ADD BASE MTL /nologo /tlb".\Release\mysql_client_test.tlb" /win32
# ADD MTL /nologo /tlb".\Release\mysql_client_test.tlb" /win32
# ADD BASE CPP /nologo /G6 /MTd /W3 /GX /Ob1 /Gy /I "../include" /I "../" /D "DBUG_OFF" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN32" /GF /c
# ADD CPP /nologo /G6 /MTd /W3 /GX /Ob1 /Gy /I "../include" /I "../" /D "DBUG_OFF" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN32" /GF /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib odbc32.lib odbccp32.lib Ws2_32.lib /nologo /subsystem:console /machine:IA64 /out:"..\tests\mysql_client_test.exe" 
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib odbc32.lib odbccp32.lib Ws2_32.lib /nologo /subsystem:console /machine:IA64 /out:"..\tests\mysql_client_test.exe" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none
# Begin Target

# Name "mysql_client_test - WinIA64 Release"
# Begin Source File

SOURCE=tests\mysql_client_test.c
# End Source File
# End Target
# End Project
