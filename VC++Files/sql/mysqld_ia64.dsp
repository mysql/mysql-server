# Microsoft Developer Studio Project File - Name="mysqld" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=mysqld - WinIA64 pro nt
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mysqld_ia64.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mysqld_ia64.mak" CFG="mysqld - WinIA64 pro nt"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mysqld - WinIA64 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - WinIA64 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - WinIA64 nt" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - WinIA64 Max nt" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - WinIA64 Max" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - WinIA64 classic" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - WinIA64 pro" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - WinIA64 classic nt" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - WinIA64 pro nt" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

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
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../zlib" /I "../include" /I "../regex" /D "NDEBUG" /D "DBUG_OFF" /D "HAVE_INNOBASE_DB" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_DLOPEN" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x410 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console  /machine:IA64
# ADD LINK32 ..\lib_release\myisammrg.lib ..\lib_release\innodb.lib ..\lib_release\zlib.lib ..\lib_release\vio.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\myisam.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib bufferoverflowU.lib /nologo /subsystem:console  /out:"../client_release/mysqld.exe" /machine:IA64
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN64" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Zi /Od /I "../bdb/build_win32" /I "../include" /I "../regex" /I "../zlib" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "HAVE_INNOBASE_DB" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_DLOPEN" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /Fr /YX
# ADD BASE RSC /l 0x410 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug  /machine:IA64
# ADD LINK32 ..\lib_debug\zlib.lib ..\lib_debug\myisammrg.lib ..\lib_debug\dbug.lib ..\lib_debug\vio.lib ..\lib_debug\isam.lib ..\lib_debug\merge.lib ..\lib_debug\mysys.lib ..\lib_debug\strings.lib ..\lib_debug\regex.lib ..\lib_debug\heap.lib ..\lib_debug\innodb.lib ..\lib_debug\myisam.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib bufferoverflowU.lib /nologo /subsystem:console /incremental:no /debug  /out:"../client_debug/mysqld-debug.exe" /machine:IA64

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqld__"
# PROP BASE Intermediate_Dir "mysqld__"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "nt"
# PROP Intermediate_Dir "nt"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE CPP /nologo /G5 /MT /W3 /O2 /I "../include" /D "WIN64" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "__WIN64__" /D "DBUG_OFF" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../regex" /I "../zlib" /D "NDEBUG" /D "__NT__" /D "DBUG_OFF" /D "MYSQL_SERVER" /D "HAVE_INNOBASE_DB" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_DLOPEN" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /DMYSQL_SERVER_SUFFIX=-nt /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x410 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib /nologo /subsystem:console /debug  /machine:IA64
# ADD LINK32 ..\lib_release\vio.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\innodb.lib ..\lib_release\zlib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib bufferoverflowU.lib /nologo /subsystem:console /map  /out:"../client_release/mysqld-nt.exe" /machine:IA64
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqld___Win64_Max_nt"
# PROP BASE Intermediate_Dir "mysqld___Win64_Max_nt"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "max_nt"
# PROP Intermediate_Dir "max_nt"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /D "NDEBUG" /D "__NT__" /D "DBUG_OFF" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../bdb/build_win32" /I "../include" /I "../regex" /I "../zlib" /D "NDEBUG" /D "__NT__" /D "DBUG_OFF" /D "HAVE_INNOBASE_DB" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_DLOPEN" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /DMYSQL_SERVER_SUFFIX=-nt-max /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\zlib.lib /nologo /subsystem:console /map  /machine:IA64
# SUBTRACT BASE LINK32 /debug
# ADD LINK32 ..\lib_release\vio.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys-max.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\zlib.lib ..\lib_release\innodb.lib ..\lib_release\mysys.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib bufferoverflowU.lib /nologo /subsystem:console /map  /out:"../client_release/mysqld-max-nt.exe" /machine:IA64
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqld___Win64_Max"
# PROP BASE Intermediate_Dir "mysqld___Win64_Max"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "max"
# PROP Intermediate_Dir "max"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /D "NDEBUG" /D "DBUG_OFF" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../bdb/build_win32" /I "../include" /I "../regex" /I "../zlib" /D "NDEBUG" /D "DBUG_OFF" /D "USE_SYMDIR" /D "HAVE_INNOBASE_DB" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_DLOPEN" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /DMYSQL_SERVER_SUFFIX=-max /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib /nologo /subsystem:console /debug  /machine:IA64
# ADD LINK32 ..\lib_release\vio.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys-max.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\innodb.lib ..\lib_release\zlib.lib ..\lib_release\mysys.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib bufferoverflowU.lib /nologo /subsystem:console  /out:"../client_release/mysqld-max.exe" /machine:IA64
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqld___Win64_classic"
# PROP BASE Intermediate_Dir "mysqld___Win64_classic"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "classic"
# PROP Intermediate_Dir "classic"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /I "../zlib" /D "DBUG_OFF" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "USE_SYMDIR" /D "HAVE_DLOPEN" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../regex" /I "../zlib" /D LICENSE=Commercial /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "HAVE_DLOPEN" /D "DBUG_OFF" /D "_MBCS" /D "NDEBUG" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /DMYSQL_SERVER_SUFFIX=-classic /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib /nologo /subsystem:console  /machine:IA64
# SUBTRACT BASE LINK32 /debug
# ADD LINK32 ..\lib_release\vio.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\zlib.lib ..\lib_release\innodb.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib bufferoverflowU.lib /nologo /subsystem:console  /out:"../client_classic/mysqld.exe" /libpath:"..\lib_release" /machine:IA64
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqld___Win64_pro"
# PROP BASE Intermediate_Dir "mysqld___Win64_pro"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "pro"
# PROP Intermediate_Dir "pro"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /I "../zlib" /D "DBUG_OFF" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "USE_SYMDIR" /D "HAVE_DLOPEN" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../regex" /I "../zlib" /D "MYSQL_SERVER" /D LICENSE=Commercial /D "_MBCS" /D "HAVE_DLOPEN" /D "HAVE_INNOBASE_DB" /D "DBUG_OFF" /D "NDEBUG" /D "_WINDOWS" /D "_CONSOLE" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /DMYSQL_SERVER_SUFFIX=-pro /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib /nologo /subsystem:console  /machine:IA64
# SUBTRACT BASE LINK32 /debug
# ADD LINK32 ..\lib_release\vio.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\innodb.lib ..\lib_release\zlib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib bufferoverflowU.lib /nologo /subsystem:console  /out:"../client_pro/mysqld.exe" /libpath:"..\lib_release" /machine:IA64
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqld___Win64_classic_nt"
# PROP BASE Intermediate_Dir "mysqld___Win64_classic_nt"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "classic_nt"
# PROP Intermediate_Dir "classic_nt"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /I "../zlib" /D "DBUG_OFF" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "USE_SYMDIR" /D "HAVE_DLOPEN" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../regex" /I "../zlib" /D "__NT__" /D "DBUG_OFF" /D "NDEBUG" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_DLOPEN" /D LICENSE=Commercial /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /DMYSQL_SERVER_SUFFIX=-classic-nt /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib /nologo /subsystem:console  /machine:IA64
# SUBTRACT BASE LINK32 /debug
# ADD LINK32 ..\lib_release\vio.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\zlib.lib ..\lib_release\innodb.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib bufferoverflowU.lib /nologo /subsystem:console  /out:"../client_classic/mysqld-nt.exe" /libpath:"..\lib_release" /machine:IA64
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqld___Win64_pro_nt"
# PROP BASE Intermediate_Dir "mysqld___Win64_pro_nt"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "pro_nt"
# PROP Intermediate_Dir "pro_nt"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
MTL=midl.exe
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /I "../zlib" /D "DBUG_OFF" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "USE_SYMDIR" /D "HAVE_DLOPEN" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../regex" /I "../zlib" /D "__NT__" /D "DBUG_OFF" /D "HAVE_INNOBASE_DB" /D LICENSE=Commercial /D "NDEBUG" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_DLOPEN" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /DMYSQL_SERVER_SUFFIX=-pro-nt" /G2 /EHsc /Wp64 /Zm600
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib /nologo /subsystem:console  /machine:IA64
# SUBTRACT BASE LINK32 /debug
# ADD LINK32 ..\lib_release\zlib.lib ..\lib_release\mysys.lib ..\lib_release\innodb.lib ..\lib_release\vio.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib bufferoverflowU.lib /nologo /subsystem:console  /out:"../client_pro/mysqld-nt.exe" /libpath:"..\lib_release" /machine:IA64
# SUBTRACT LINK32 /debug

!ENDIF 

# Begin Target

# Name "mysqld - WinIA64 Release"
# Name "mysqld - WinIA64 Debug"
# Name "mysqld - WinIA64 nt"
# Name "mysqld - WinIA64 Max nt"
# Name "mysqld - WinIA64 Max"
# Name "mysqld - WinIA64 classic"
# Name "mysqld - WinIA64 pro"
# Name "mysqld - WinIA64 classic nt"
# Name "mysqld - WinIA64 pro nt"
# Begin Source File

SOURCE=.\client.c

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\derror.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\discover.cpp
# End Source File
# Begin Source File

SOURCE=..\libmysql\errmsg.c
# End Source File
# Begin Source File

SOURCE=.\field.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\field_conv.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\filesort.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gstream.cpp
# End Source File
# Begin Source File

SOURCE=.\ha_berkeley.cpp
# End Source File
# Begin Source File

SOURCE=.\ha_heap.cpp
# End Source File
# Begin Source File

SOURCE=.\ha_innodb.cpp
# End Source File
# Begin Source File

SOURCE=.\ha_isam.cpp
# End Source File
# Begin Source File

SOURCE=.\ha_isammrg.cpp
# End Source File
# Begin Source File

SOURCE=.\ha_myisam.cpp
# End Source File
# Begin Source File

SOURCE=.\ha_myisammrg.cpp
# End Source File
# Begin Source File

SOURCE=.\handler.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\hash_filo.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\hash_filo.h
# End Source File
# Begin Source File

SOURCE=.\hostname.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\init.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\item.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\item_buff.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\item_cmpfunc.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\item_create.cpp
# End Source File
# Begin Source File

SOURCE=.\item_func.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\item_geofunc.cpp
# End Source File
# Begin Source File

SOURCE=.\item_row.cpp
# End Source File
# Begin Source File

SOURCE=.\item_strfunc.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\item_subselect.cpp
# End Source File
# Begin Source File

SOURCE=.\item_sum.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\item_timefunc.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\item_uniq.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\key.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\lock.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\log.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\log_event.cpp
# End Source File
# Begin Source File

SOURCE=.\message.mc

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\message.rc
# End Source File
# Begin Source File

SOURCE=.\mf_iocache.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\my_time.c
# End Source File
# Begin Source File

SOURCE=..\myisammrg\myrg_rnext_same.c
# End Source File
# Begin Source File

SOURCE=.\mysqld.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\net_serv.cpp
# End Source File
# Begin Source File

SOURCE=.\nt_servc.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\nt_servc.h
# End Source File
# Begin Source File

SOURCE=.\opt_range.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\opt_range.h
# End Source File
# Begin Source File

SOURCE=.\OPT_SUM.cpp
# End Source File
# Begin Source File

SOURCE=.\pack.c
# End Source File
# Begin Source File

SOURCE=.\password.c

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\procedure.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\protocol.cpp
# End Source File
# Begin Source File

SOURCE=.\records.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\repl_failsafe.cpp
# End Source File
# Begin Source File

SOURCE=.\set_var.cpp
# End Source File
# Begin Source File

SOURCE=.\slave.cpp
# End Source File
# Begin Source File

SOURCE=.\spatial.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_acl.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_analyse.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_base.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_cache.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_class.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_client.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_crypt.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_crypt.h
# End Source File
# Begin Source File

SOURCE=.\sql_db.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_delete.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_derived.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_do.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_error.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_handler.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_help.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_insert.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_lex.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_list.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_load.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_locale.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_manager.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_map.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_parse.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_prepare.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_rename.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_repl.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_select.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_show.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_state.c
# End Source File
# Begin Source File

SOURCE=.\sql_string.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_table.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_test.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_udf.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_union.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_update.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_yacc.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\strfunc.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\table.cpp
# End Source File
# Begin Source File

SOURCE=.\thr_malloc.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\time.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\tztime.cpp
# End Source File
# Begin Source File

SOURCE=.\uniques.cpp
# End Source File
# Begin Source File

SOURCE=.\unireg.cpp

!IF  "$(CFG)" == "mysqld - WinIA64 Release"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Debug"

# ADD CPP /G5 /Zi /Od /G2 /EHsc /Wp64 /Zm600
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 Max"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - WinIA64 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\sql-common\my_user.c
# End Source File
# End Target
# End Project
