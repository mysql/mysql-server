# Microsoft Developer Studio Project File - Name="mysql_test_run_new" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=mysql_test_run_new - WinIA64 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mysql_test_run_new.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mysql_test_run_new.mak" CFG="mysql_test_run_new - WinIA64 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mysql_test_run_new - WinIA64 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "mysql_test_run_new - WinIA64 Release" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mysql_test_run_new - WinIA64 Debug"

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
MTL=midl.exe
# ADD BASE MTL /nologo /tlb".\Debug\mysql_test_run_new.tlb" /WinIA64
# ADD MTL /nologo /tlb".\Debug\mysql_test_run_new.tlb" /WinIA64
# ADD BASE CPP /nologo /G6 /MTd /W3 /GX /Z7 /Od /I "../include" /I "../" /D "_DEBUG" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN64" /GZ /c
# ADD CPP /nologo /MTd /W3 /Zi /Od /I "../include" /I "../" /D "_DEBUG" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN64" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /GZ /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib  odbc32.lib odbccp32.lib  Ws2_32.lib /nologo /subsystem:console /map /debug /machine:IA64 /out:"..\mysql-test\mysql_test_run_new.exe" 
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib  odbc32.lib odbccp32.lib  Ws2_32.lib /nologo /subsystem:console /incremental:no /map /debug /machine:IA64 /out:"..\mysql-test\mysql_test_run_new.exe"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "mysql_test_run_new - WinIA64 Release"

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
# ADD BASE MTL /nologo /tlb".\Release\mysql_test_run_new.tlb" /WinIA64
# ADD MTL /nologo /tlb".\Release\mysql_test_run_new.tlb" /WinIA64
# ADD BASE CPP /nologo /G6 /MTd /W3 /GX /Ob1 /Gy /I "../include" /I "../" /D "DBUG_OFF" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN64" /GF /c
# ADD CPP /nologo /MTd /W3 /Zi /O2 /I "../include" /I "../" /D "DBUG_OFF" /D "_WINDOWS" /D "SAFE_MUTEX" /D "USE_TLS" /D "MYSQL_CLIENT" /D "__WIN__" /D "_WIN64" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /GF /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib  odbc32.lib odbccp32.lib  Ws2_32.lib /nologo /subsystem:console /machine:IA64 /out:"..\mysql-test\mysql_test_run_new.exe" 
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib  odbc32.lib odbccp32.lib  Ws2_32.lib /nologo /subsystem:console /machine:IA64 /out:"..\mysql-test\mysql_test_run_new.exe" t
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "mysql_test_run_new - WinIA64 Debug"
# Name "mysql_test_run_new - WinIA64 Release"
# Begin Source File

SOURCE=.\my_create_tables.c
DEP_CPP_MY_CR=\
	"..\include\config-netware.h"\
	"..\include\config-os2.h"\
	"..\include\config-win.h"\
	"..\include\m_string.h"\
	"..\include\my_config.h"\
	"..\include\my_dbug.h"\
	"..\include\my_global.h"\
	".\my_manage.h"\
	
# End Source File
# Begin Source File

SOURCE=.\my_manage.c
DEP_CPP_MY_MA=\
	"..\include\config-netware.h"\
	"..\include\config-os2.h"\
	"..\include\config-win.h"\
	"..\include\m_string.h"\
	"..\include\my_config.h"\
	"..\include\my_dbug.h"\
	"..\include\my_global.h"\
	".\my_manage.h"\
	
# End Source File
# Begin Source File

SOURCE=.\my_manage.h
# End Source File
# Begin Source File

SOURCE=.\mysql_test_run_new.c
DEP_CPP_MYSQL=\
	"..\include\config-netware.h"\
	"..\include\config-os2.h"\
	"..\include\config-win.h"\
	"..\include\m_string.h"\
	"..\include\my_config.h"\
	"..\include\my_dbug.h"\
	"..\include\my_global.h"\
	".\my_manage.h"\
	
# End Source File
# End Target
# End Project
