# Microsoft Developer Studio Project File - Name="mysql_test_run_new" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=mysql_test_run_new - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mysql_test_run_new.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mysql_test_run_new.mak" CFG="mysql_test_run_new - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mysql_test_run_new - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "mysql_test_run_new - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mysql_test_run_new - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /I "../include" /I "../" /Z7 /W3 /Od /G6 /D "_DEBUG" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN32" /Fp".\debug/mysql_test_run.pch" /Fo".\debug/" /Fd".\debug/" /GZ /c /GX 
# ADD CPP /nologo /MTd /I "../include" /I "../" /Z7 /W3 /Od /G6 /D "_DEBUG" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN32" /Fp".\debug/mysql_test_run.pch" /Fo".\debug/" /Fd".\debug/" /GZ /c /GX 
# ADD BASE MTL /nologo /win32 
# ADD MTL /nologo /win32 
# ADD BASE RSC /l 1033 
# ADD RSC /l 1033 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo 
# ADD BSC32 /nologo 
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Ws2_32.lib /nologo /out:"..\mysql-test\mysql_test_run_new.exe" /debug /pdb:".\debug\mysql_test_run_new.pdb" /pdbtype:sept /map /mapinfo:exports /subsystem:windows 
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Ws2_32.lib /nologo /out:"..\mysql-test\mysql_test_run_new.exe" /debug /pdb:".\debug\mysql_test_run_new.pdb" /pdbtype:sept /map /mapinfo:exports /subsystem:windows 

!ELSEIF  "$(CFG)" == "mysql_test_run_new - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /I "../include" /I "../" /W3 /Ob1 /G6 /D "DBUG_OFF" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN32" /GF /Gy /Fo".\release/" /Fd".\release/" /c /GX 
# ADD CPP /nologo /MTd /I "../include" /I "../" /W3 /Ob1 /G6 /D "DBUG_OFF" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN32" /GF /Gy /Fo".\release/" /Fd".\release/" /c /GX 
# ADD BASE MTL /nologo /win32 
# ADD MTL /nologo /win32 
# ADD BASE RSC /l 1033 
# ADD RSC /l 1033 
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo 
# ADD BSC32 /nologo 
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Ws2_32.lib /nologo /out:"..\mysql-test\mysql_test_run_new.exe" /pdbtype:sept /subsystem:windows 
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Ws2_32.lib /nologo /out:"..\mysql-test\mysql_test_run_new.exe" /pdbtype:sept /subsystem:windows 

!ENDIF

# Begin Target

# Name "mysql_test_run_new - Win32 Debug"
# Name "mysql_test_run_new - Win32 Release"
# Begin Source File

SOURCE=my_create_tables.c
# End Source File
# Begin Source File

SOURCE=my_manage.c
# End Source File
# Begin Source File

SOURCE=my_manage.h
# End Source File
# Begin Source File

SOURCE=mysql_test_run_new.c
# End Source File
# End Target
# End Project

