# Microsoft Developer Studio Project File - Name="mysqld" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=mysqld - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "mysqld.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "mysqld.mak" CFG="mysqld - Win32 Release"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "mysqld - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - Win32 nt" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - Win32 Max nt" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - Win32 Max" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - Win32 classic" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - Win32 pro" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - Win32 classic nt" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqld - Win32 pro nt" (based on "Win32 (x86) Console Application")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl6.exe
RSC=rc.exe

!IF  "$(CFG)" == "mysqld - Win32 Release"

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
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../zlib" /I "../include" /I "../regex" /I "../extra/yassl/include" /D "NDEBUG" /D "DBUG_OFF" /D "HAVE_INNOBASE_DB" /D "HAVE_ARCHIVE_DB" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_DLOPEN" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x410 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\extra\yassl\Release\yassl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"../client_release/mysqld.exe"
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G6 /MTd /W3 /Z7 /Od /I "../bdb/build_win32" /I "../include" /I "../regex" /I "../extra/yassl/include" /I "../zlib" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "HAVE_INNOBASE_DB" /D "HAVE_BERKELEY_DB" /D "HAVE_ARCHIVE_DB" /D "HAVE_BLACKHOLE_DB" /D "HAVE_FEDERATED_DB" /D "HAVE_EXAMPLE_DB" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_DLOPEN" /FD /c
# SUBTRACT CPP /Fr /YX
# ADD BASE RSC /l 0x410 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_debug\dbug.lib ..\lib_debug\vio.lib ..\lib_debug\mysys.lib ..\lib_debug\strings.lib ..\lib_debug\regex.lib ..\lib_debug\heap.lib ..\lib_debug\bdb.lib ..\lib_debug\innodb.lib ..\extra\yassl\Debug\yassl.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"../client_debug/mysqld-debug.exe" /pdbtype:sept

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

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
# ADD BASE CPP /nologo /G5 /MT /W3 /O2 /I "../include" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "__WIN32__" /D "DBUG_OFF" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /I "../zlib" /I "../extra/yassl/include" /D "__NT__" /D "DBUG_OFF" /D "NDEBUG" /D "HAVE_INNOBASE_DB" /D "HAVE_ARCHIVE_DB" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_DLOPEN" /D MYSQL_SERVER_SUFFIX=-nt /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x410 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib /nologo /subsystem:console /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\innodb.lib ..\lib_release\zlib.lib ..\extra\yassl\Release\yassl.lib /nologo /subsystem:console /map /machine:I386 /out:"../client_release/mysqld-nt.exe"
# SUBTRACT LINK32 /pdb:none /debug

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqld___Win32_Max_nt"
# PROP BASE Intermediate_Dir "mysqld___Win32_Max_nt"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "max_nt"
# PROP Intermediate_Dir "max_nt"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /D "NDEBUG" /D "__NT__" /D "DBUG_OFF" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../bdb/build_win32" /I "../include" /I "../regex" /I "../extra/yassl/include" /I "../zlib" /D "NDEBUG" /D "__NT__" /D "DBUG_OFF" /D "HAVE_INNOBASE_DB" /D "HAVE_BERKELEY_DB" /D "HAVE_ARCHIVE_DB" /D "HAVE_BLACKHOLE_DB" /D "HAVE_EXAMPLE_DB" /D "HAVE_FEDERATED_DB" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_DLOPEN" /D MYSQL_SERVER_SUFFIX=-nt-max /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\zlib.lib /nologo /subsystem:console /map /machine:I386
# SUBTRACT BASE LINK32 /pdb:none /debug
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys-max.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\zlib.lib ..\lib_release\innodb.lib ..\lib_release\bdb.lib ..\extra\yassl\Release\yassl.lib /nologo /subsystem:console /map /machine:I386 /out:"../client_release/mysqld-max-nt.exe"
# SUBTRACT LINK32 /pdb:none /debug

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqld___Win32_Max"
# PROP BASE Intermediate_Dir "mysqld___Win32_Max"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "max"
# PROP Intermediate_Dir "max"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /D "NDEBUG" /D "DBUG_OFF" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../bdb/build_win32" /I "../include" /I "../regex" /I "../extra/yassl/include" /I "../zlib" /D "NDEBUG" /D "DBUG_OFF" /D "USE_SYMDIR" /D "HAVE_INNOBASE_DB" /D "HAVE_BERKELEY_DB" /D "HAVE_ARCHIVE_DB" /D "HAVE_BLACKHOLE_DB" /D "HAVE_EXAMPLE_DB" /D "HAVE_FEDERATED_DB" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_DLOPEN" /D MYSQL_SERVER_SUFFIX=-max /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib /nologo /subsystem:console /pdb:none /debug /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys-max.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\innodb.lib ..\lib_release\bdb.lib ..\lib_release\zlib.lib ..\extra\yassl\Release\yassl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"../client_release/mysqld-max.exe"
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqld___Win32_classic"
# PROP BASE Intermediate_Dir "mysqld___Win32_classic"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "classic"
# PROP Intermediate_Dir "classic"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /I "../zlib" /D "DBUG_OFF" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "USE_SYMDIR" /D "HAVE_DLOPEN" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /I "../zlib" /I "../extra/yassl/include" /D LICENSE=Commercial /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "HAVE_DLOPEN" /D "DBUG_OFF" /D "_MBCS" /D "NDEBUG" /FD /D MYSQL_SERVER_SUFFIX=-classic /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib /nologo /subsystem:console /pdb:none /machine:I386
# SUBTRACT BASE LINK32 /debug
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\zlib.lib ..\extra\yassl\Release\yassl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"../client_classic/mysqld.exe" /libpath:"..\lib_release"
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqld___Win32_pro"
# PROP BASE Intermediate_Dir "mysqld___Win32_pro"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "pro"
# PROP Intermediate_Dir "pro"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /I "../zlib" /D "DBUG_OFF" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "USE_SYMDIR" /D "HAVE_DLOPEN" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /I "../zlib" /I "../extra/yassl/include" /D "MYSQL_SERVER" /D LICENSE=Commercial /D "_MBCS" /D "HAVE_DLOPEN" /D "HAVE_INNOBASE_DB" /D "HAVE_ARCHIVE_DB" /D "DBUG_OFF" /D "NDEBUG" /D "_WINDOWS" /D "_CONSOLE" /D MYSQL_SERVER_SUFFIX=-pro /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib /nologo /subsystem:console /pdb:none /machine:I386
# SUBTRACT BASE LINK32 /debug
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\innodb.lib ..\lib_release\zlib.lib ..\extra\yassl\Release\yassl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"../client_pro/mysqld.exe" /libpath:"..\lib_release"
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqld___Win32_classic_nt"
# PROP BASE Intermediate_Dir "mysqld___Win32_classic_nt"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "classic_nt"
# PROP Intermediate_Dir "classic_nt"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /I "../zlib" /D "DBUG_OFF" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "USE_SYMDIR" /D "HAVE_DLOPEN" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /I "../zlib" /I "../extra/yassl/include" /D "__NT__" /D "DBUG_OFF" /D "NDEBUG" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_DLOPEN" /FD /D LICENSE=Commercial /D MYSQL_SERVER_SUFFIX=-classic-nt /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib /nologo /subsystem:console /pdb:none /machine:I386
# SUBTRACT BASE LINK32 /debug
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib  ..\lib_release\zlib.lib ..\extra\yassl\Release\yassl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"../client_classic/mysqld-nt.exe" /libpath:"..\lib_release"
# SUBTRACT LINK32 /debug

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqld___Win32_pro_nt"
# PROP BASE Intermediate_Dir "mysqld___Win32_pro_nt"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "pro_nt"
# PROP Intermediate_Dir "pro_nt"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /I "../zlib" /D "DBUG_OFF" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "USE_SYMDIR" /D "HAVE_DLOPEN" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /I "../zlib" /I "../extra/yassl/include" /D "__NT__" /D "DBUG_OFF" /D "NDEBUG" /D "HAVE_INNOBASE_DB" /D "HAVE_ARCHIVE_DB" /D "MYSQL_SERVER" /D LICENSE=Commercial /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_DLOPEN" /D MYSQL_SERVER_SUFFIX=-pro-nt /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib /nologo /subsystem:console /pdb:none /machine:I386
# SUBTRACT BASE LINK32 /debug
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\vio.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\innodb.lib ..\lib_release\zlib.lib ..\extra\yassl\Release\yassl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"../client_pro/mysqld-nt.exe" /libpath:"..\lib_release"
# SUBTRACT LINK32 /debug

!ENDIF

# Begin Target

# Name "mysqld - Win32 Release"
# Name "mysqld - Win32 Debug"
# Name "mysqld - Win32 nt"
# Name "mysqld - Win32 Max nt"
# Name "mysqld - Win32 Max"
# Name "mysqld - Win32 classic"
# Name "mysqld - Win32 pro"
# Name "mysqld - Win32 classic nt"
# Name "mysqld - Win32 pro nt"
# Begin Source File

SOURCE=.\client.c

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\derror.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\des_key_file.cpp
# End Source File
# Begin Source File

SOURCE=.\discover.cpp
# End Source File
# Begin Source File

SOURCE=..\libmysql\errmsg.c
# End Source File
# Begin Source File

SOURCE=.\field.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\field_conv.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\filesort.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\gstream.cpp
# End Source File
# Begin Source File

SOURCE=.\examples\ha_archive.cpp
# End Source File
# Begin Source File

SOURCE=.\examples\ha_example.cpp
# End Source File
# Begin Source File

SOURCE=.\ha_blackhole.cpp
# End Source File
# Begin Source File

SOURCE=.\ha_federated.cpp
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

SOURCE=.\ha_myisam.cpp
# End Source File
# Begin Source File

SOURCE=.\ha_myisammrg.cpp
# End Source File
# Begin Source File

SOURCE=.\handler.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\hash_filo.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\hash_filo.h
# End Source File
# Begin Source File

SOURCE=.\hostname.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\init.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\item.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\item_buff.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\item_cmpfunc.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\item_create.cpp
# End Source File
# Begin Source File

SOURCE=.\item_func.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

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

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\item_subselect.cpp
# End Source File
# Begin Source File

SOURCE=.\item_sum.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\item_timefunc.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\item_uniq.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\key.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\lock.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\log.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\log_event.cpp
# End Source File
# Begin Source File

SOURCE=.\message.mc

!IF  "$(CFG)" == "mysqld - Win32 Release"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

# Begin Custom Build
InputPath=.\message.mc

BuildCmds= \
	mc message.mc

"message.rc" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)

"message.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
   $(BuildCmds)
# End Custom Build

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

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

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\my_decimal.cpp
# End Source File
# Begin Source File

SOURCE=.\my_time.c
# End Source File
# Begin Source File

SOURCE=.\mysqld.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\net_serv.cpp
# End Source File
# Begin Source File

SOURCE=.\nt_servc.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\nt_servc.h
# End Source File
# Begin Source File

SOURCE=.\opt_range.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

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

SOURCE=.\parse_file.cpp
# End Source File
# Begin Source File

SOURCE=.\password.c

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\procedure.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\protocol.cpp
# End Source File
# Begin Source File

SOURCE=.\records.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

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

SOURCE=.\sp.cpp
# End Source File
# Begin Source File

SOURCE=.\sp_cache.cpp
# End Source File
# Begin Source File

SOURCE=.\sp_head.cpp
# End Source File
# Begin Source File

SOURCE=.\sp_pcontext.cpp
# End Source File
# Begin Source File

SOURCE=.\sp_rcontext.cpp
# End Source File
# Begin Source File

SOURCE=.\spatial.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_acl.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_analyse.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_base.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_cache.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_class.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

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

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_delete.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

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

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_lex.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_list.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_load.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_manager.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_map.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_parse.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

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

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_show.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_state.c
# End Source File
# Begin Source File

SOURCE=.\sql_string.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_table.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_test.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_trigger.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_udf.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_union.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_update.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sql_view.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_yacc.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\strfunc.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\table.cpp
# End Source File
# Begin Source File

SOURCE=.\thr_malloc.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\time.cpp

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

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

!IF  "$(CFG)" == "mysqld - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqld - Win32 nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 Max"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro"

!ELSEIF  "$(CFG)" == "mysqld - Win32 classic nt"

!ELSEIF  "$(CFG)" == "mysqld - Win32 pro nt"

!ENDIF

# End Source File
# End Target
# End Project
