# Microsoft Developer Studio Project File - Name="mysqltest" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=MYSQLTEST - WinIA64 RELEASE
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mysqltest_ia64.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mysqltest_ia64.mak" CFG="MYSQLTEST - WinIA64 RELEASE"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mysqltest - WinIA64 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqltest - WinIA64 classic" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqltest - WinIA64 Release" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mysqltest - WinIA64 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir ".\debug"
# PROP BASE Intermediate_Dir ".\debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir ".\debug"
# PROP Intermediate_Dir ".\debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE MTL /nologo /tlb".\debug\mysqltest.tlb" /win64
# ADD MTL /nologo /tlb".\debug\mysqltest.tlb" /win64
# ADD BASE CPP /nologo /G6 /MTd /W3 /GX /Z7 /Od /I "../include" /I "../regex" /I "../" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_CONSOLE" /D "_WINDOWS" /D "_MBCS" /GZ /c
# ADD CPP /nologo /MTd /W3 /Zi /Od /I "../include" /I "../regex" /I "../" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_CONSOLE" /D "_WINDOWS" /D "_MBCS" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib odbc32.lib odbccp32.lib mysqlclient.lib wsock32.lib mysys.lib regex.lib /nologo /subsystem:console /debug  /out:"..\client_debug\mysqltest.exe" /libpath:"..\lib_debug\\"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 ..\lib_debug\zlib.lib ..\lib_debug\dbug.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysqlclient.lib wsock32.lib mysys.lib regex.lib bufferoverflowU.lib /nologo /subsystem:console /incremental:no /debug  /out:"..\client_debug\mysqltest.exe" /libpath:"..\lib_debug\\" /machine:IA64
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "mysqltest - WinIA64 classic"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\classic"
# PROP BASE Intermediate_Dir ".\classic"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\classic"
# PROP Intermediate_Dir ".\classic"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE MTL /nologo /tlb".\classic\mysqltest.tlb" /win64
# ADD MTL /nologo /tlb".\classic\mysqltest.tlb" /win64
# ADD BASE CPP /nologo /G6 /MT /W3 /GX /Ob1 /Gy /I "../include" /I "../regex" /I "../" /D "_CONSOLE" /D "_WINDOWS" /D LICENSE=Commercial /D "DBUG_OFF" /D "NDEBUG" /D "_MBCS" /GF /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../regex" /I "../" /D "_CONSOLE" /D "_WINDOWS" /D LICENSE=Commercial /D "DBUG_OFF" /D "NDEBUG" /D "_MBCS" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /GF /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib odbc32.lib odbccp32.lib mysqlclient.lib wsock32.lib mysys.lib regex.lib /nologo /subsystem:console  /out:"..\client_classic\mysqltest.exe" /libpath:"..\lib_release\\"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 ..\lib_release\zlib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysqlclient.lib wsock32.lib mysys.lib regex.lib bufferoverflowU.lib /nologo /subsystem:console  /out:"..\client_classic\mysqltest.exe" /libpath:"..\lib_release\\" /machine:IA64
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "mysqltest - WinIA64 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir ".\release"
# PROP BASE Intermediate_Dir ".\release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir ".\release"
# PROP Intermediate_Dir ".\release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE MTL /nologo /tlb".\release\mysqltest.tlb" /win64
# ADD MTL /nologo /tlb".\release\mysqltest.tlb" /win64
# ADD BASE CPP /nologo /G6 /MT /W3 /GX /Ob1 /Gy /I "../include" /I "../regex" /I "../" /D "DBUG_OFF" /D "_CONSOLE" /D "_WINDOWS" /D "NDEBUG" /D "_MBCS" /GF /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../regex" /I "../" /D "DBUG_OFF" /D "_CONSOLE" /D "_WINDOWS" /D "NDEBUG" /D "_MBCS" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /G2 /GF /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib odbc32.lib odbccp32.lib mysqlclient.lib wsock32.lib mysys.lib regex.lib /nologo /subsystem:console  /out:"..\client_release\mysqltest.exe" /libpath:"..\lib_release\\"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 ..\lib_release\zlib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mysqlclient.lib wsock32.lib mysys.lib regex.lib bufferoverflowU.lib /nologo /subsystem:console  /out:"..\client_release\mysqltest.exe" /libpath:"..\lib_release\\" /machine:IA64
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "mysqltest - WinIA64 Debug"
# Name "mysqltest - WinIA64 classic"
# Name "mysqltest - WinIA64 Release"
# Begin Source File

SOURCE=..\libmysql\manager.c
DEP_CPP_MANAG=\
	"..\include\config-netware.h"\
	"..\include\config-os2.h"\
	"..\include\config-win.h"\
	"..\include\errmsg.h"\
	"..\include\m_ctype.h"\
	"..\include\m_string.h"\
	"..\include\my_alloc.h"\
	"..\include\my_config.h"\
	"..\include\my_dbug.h"\
	"..\include\my_dir.h"\
	"..\include\my_global.h"\
	"..\include\my_list.h"\
	"..\include\my_net.h"\
	"..\include\my_pthread.h"\
	"..\include\my_sys.h"\
	"..\include\mysql.h"\
	"..\include\mysql_com.h"\
	"..\include\mysql_time.h"\
	"..\include\mysql_version.h"\
	"..\include\mysqld_error.h"\
	"..\include\mysys_err.h"\
	"..\include\raid.h"\
	"..\include\t_ctype.h"\
	"..\include\typelib.h"\
	"..\include\violite.h"\
	
# End Source File
# Begin Source File

SOURCE=.\mysqltest.c
DEP_CPP_MYSQL=\
	"..\include\config-netware.h"\
	"..\include\config-os2.h"\
	"..\include\config-win.h"\
	"..\include\errmsg.h"\
	"..\include\hash.h"\
	"..\include\help_end.h"\
	"..\include\help_start.h"\
	"..\include\m_ctype.h"\
	"..\include\m_string.h"\
	"..\include\my_alloc.h"\
	"..\include\my_config.h"\
	"..\include\my_dbug.h"\
	"..\include\my_dir.h"\
	"..\include\my_getopt.h"\
	"..\include\my_global.h"\
	"..\include\my_list.h"\
	"..\include\my_net.h"\
	"..\include\my_pthread.h"\
	"..\include\my_sys.h"\
	"..\include\mysql.h"\
	"..\include\mysql_com.h"\
	"..\include\mysql_embed.h"\
	"..\include\mysql_time.h"\
	"..\include\mysql_version.h"\
	"..\include\mysqld_error.h"\
	"..\include\raid.h"\
	"..\include\sslopt-case.h"\
	"..\include\sslopt-longopts.h"\
	"..\include\sslopt-vars.h"\
	"..\include\t_ctype.h"\
	"..\include\typelib.h"\
	"..\include\violite.h"\
	"..\regex\regex.h"\
	
# End Source File
# End Target
# End Project
