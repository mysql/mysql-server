# Microsoft Developer Studio Project File - Name="libmysqld" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=libmysqld - WinIA64 pro
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libmysqld_ia64.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libmysqld_ia64.mak" CFG="libmysqld - WinIA64 pro"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libmysqld - WinIA64 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libmysqld - WinIA64 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libmysqld - WinIA64 classic" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libmysqld - WinIA64 pro" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libmysqld - WinIA64 Release"

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
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN64" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBMYSQLD_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../libmysqld" /I "../sql" /I "../regex" /I "../zlib" /D "WIN64" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "SIGNAL_WITH_VIO_CLOSE" /D "HAVE_DLOPEN" /D "EMBEDDED_LIBRARY" /D "HAVE_INNOBASE_DB" /D "DBUG_OFF" /D "USE_TLS" /D "__WIN__" /D "_IA64_" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /WX /Fr
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win64
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win64
# ADD BASE RSC /l 0x416 /d "NDEBUG"
# ADD RSC /l 0x416 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll  /machine:IA64
# ADD LINK32 ..\lib_release\myisam_tls.lib ..\lib_release\myisammrg_tls.lib ..\lib_release\mysys_tls.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap_tls.lib ..\lib_release\innodb.lib ..\lib_release\zlib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib bufferoverflowU.lib /nologo /dll  /out:"../lib_release/libmysqld.dll" /implib:"../lib_release/libmysqld.lib" /machine:IA64

!ELSEIF  "$(CFG)" == "libmysqld - WinIA64 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "libmysqld___Win64_Debug"
# PROP BASE Intermediate_Dir "libmysqld___Win64_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN64" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBMYSQLD_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MT /W3 /Zi /Od /I "../include" /I "../libmysqld" /I "../sql" /I "../regex" /I "../zlib" /D "WIN64" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "SAFEMALLOC" /D "USE_SYMDIR" /D "SIGNAL_WITH_VIO_CLOSE" /D "HAVE_DLOPEN" /D "EMBEDDED_LIBRARY" /D "HAVE_INNOBASE_DB" /D "USE_TLS" /D "__WIN__" /D "_IA64_" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win64
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win64
# ADD BASE RSC /l 0x416 /d "_DEBUG"
# ADD RSC /l 0x416 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug  /machine:IA64
# ADD LINK32 ..\lib_debug\dbug_tls.lib ..\lib_debug\myisam_tls.lib ..\lib_debug\myisammrg_tls.lib ..\lib_debug\mysys_tls.lib ..\lib_debug\strings.lib ..\lib_debug\regex.lib ..\lib_debug\heap_tls.lib ..\lib_debug\innodb.lib ../lib_debug\zlib.lib bufferoverflowU.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib /nologo /dll /incremental:no /debug  /nodefaultlib:"LIBCMTD" /out:"../lib_debug/libmysqld.dll" /implib:"../lib_debug/libmysqld.lib" /machine:IA64

!ELSEIF  "$(CFG)" == "libmysqld - WinIA64 classic"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "libmysqld___Win64_classic"
# PROP BASE Intermediate_Dir "libmysqld___Win64_classic"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "classic"
# PROP Intermediate_Dir "classic"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /I "../sql" /D "WIN64" /D "_WINDOWS" /D "_MBCS" /D "USE_SYMDIR" /D "SIGNAL_WITH_VIO_CLOSE" /D "HAVE_DLOPEN" /D "EMBEDDED_LIBRARY" /D "DBUG_OFF" /D "USE_TLS" /D "__WIN__" /D "NDEBUG" /FR /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../regex" /I "../libmysqld" /I "../sql" /I "../zlib" /D "WIN64" /D "_WINDOWS" /D "USE_SYMDIR" /D "SIGNAL_WITH_VIO_CLOSE" /D "HAVE_DLOPEN" /D "EMBEDDED_LIBRARY" /D "USE_TLS" /D "__WIN__" /D LICENSE=Commerical /D "DBUG_OFF" /D "_MBCS" /D "NDEBUG" /D "_IA64_" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /Fr
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win64
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win64
# ADD BASE RSC /l 0x416 /d "NDEBUG"
# ADD RSC /l 0x416 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\myisam_tls.lib ..\lib_release\myisammrg_tls.lib ..\lib_release\mysys_tls.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap_tls.lib ..\lib_release\zlib.lib /nologo /dll  /out:"../lib_release/libmysqld.dll" /implib:"../lib_release/libmysqld.lib" /machine:IA64
# ADD LINK32 ..\lib_release\myisam_tls.lib ..\lib_release\myisammrg_tls.lib ..\lib_release\mysys_tls.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap_tls.lib ..\lib_release\zlib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib bufferoverflowU.lib /nologo /dll  /out:"../lib_classic/libmysqld.dll" /implib:"../lib_release/libmysqld.lib" /machine:IA64

!ELSEIF  "$(CFG)" == "libmysqld - WinIA64 pro"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "libmysqld___Win64_pro"
# PROP BASE Intermediate_Dir "libmysqld___Win64_pro"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "pro"
# PROP Intermediate_Dir "pro"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /I "../sql" /D "WIN64" /D "_WINDOWS" /D "_MBCS" /D "USE_SYMDIR" /D "SIGNAL_WITH_VIO_CLOSE" /D "HAVE_DLOPEN" /D "EMBEDDED_LIBRARY" /D "HAVE_INNOBASE_DB" /D "DBUG_OFF" /D "USE_TLS" /D "__WIN__" /D "NDEBUG" /FR /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../regex" /I "../libmysqld" /I "../sql" /I "../zlib" /D "WIN64" /D "USE_SYMDIR" /D "SIGNAL_WITH_VIO_CLOSE" /D "EMBEDDED_LIBRARY" /D "USE_TLS" /D "__WIN__" /D "MYSQL_SERVER" /D LICENSE=Commercial /D "_MBCS" /D "HAVE_DLOPEN" /D "HAVE_INNOBASE_DB" /D "DBUG_OFF" /D "NDEBUG" /D "_WINDOWS" /D "_CONSOLE" /D "_IA64_" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /DMYSQL_SERVER_SUFFIX=-pro /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /X /Fr
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win64
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win64
# ADD BASE RSC /l 0x416 /d "NDEBUG"
# ADD RSC /l 0x416 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\myisam_tls.lib ..\lib_release\myisammrg_tls.lib ..\lib_release\mysys_tls.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap_tls.lib ..\lib_release\innodb.lib ..\lib_release\zlib.lib /nologo /dll  /out:"../lib_classic/libmysqld.dll" /implib:"../lib_release/libmysqld.lib" /machine:IA64
# ADD LINK32 ..\lib_release\myisam_tls.lib ..\lib_release\myisammrg_tls.lib ..\lib_release\mysys_tls.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap_tls.lib ..\lib_release\innodb.lib ..\lib_release\zlib.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib bufferoverflowU.lib /nologo /dll  /out:"../lib_pro/libmysqld.dll" /implib:"../lib_release/libmysqld.lib" /machine:IA64

!ENDIF 

# Begin Target

# Name "libmysqld - WinIA64 Release"
# Name "libmysqld - WinIA64 Debug"
# Name "libmysqld - WinIA64 classic"
# Name "libmysqld - WinIA64 pro"
# Begin Source File

SOURCE="..\sql-common\client.c"
# End Source File
# Begin Source File

SOURCE="..\strings\ctype-latin1.c"
# End Source File
# Begin Source File

SOURCE=..\mysys\default.c
# End Source File
# Begin Source File

SOURCE=..\sql\derror.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\discover.cpp
# End Source File
# Begin Source File

SOURCE=.\emb_qcache.cpp
# End Source File
# Begin Source File

SOURCE=..\libmysql\errmsg.c
# End Source File
# Begin Source File

SOURCE=..\sql\field.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\field_conv.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\filesort.cpp
# End Source File
# Begin Source File

SOURCE=..\client\get_password.c
# End Source File
# Begin Source File

SOURCE=..\sql\gstream.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\ha_berkeley.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\ha_heap.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\ha_innodb.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\ha_isammrg.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\ha_myisam.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\ha_myisammrg.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\handler.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\hash_filo.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\hostname.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\init.cpp
# End Source File
# Begin Source File

SOURCE=..\strings\int2str.c
# End Source File
# Begin Source File

SOURCE=..\sql\item.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\item_buff.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\item_cmpfunc.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\item_create.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\item_func.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\item_geofunc.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\item_row.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\item_strfunc.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\item_subselect.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\item_sum.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\item_timefunc.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\item_uniq.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\key.cpp
# End Source File
# Begin Source File

SOURCE=.\lib_sql.cpp
# End Source File
# Begin Source File

SOURCE=..\libmysql\libmysql.c
# End Source File
# Begin Source File

SOURCE=.\libmysqld.c
# End Source File
# Begin Source File

SOURCE=.\libmysqld.def
# End Source File
# Begin Source File

SOURCE=..\sql\lock.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\log.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\log_event.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\mf_iocache.cpp
# End Source File
# Begin Source File

SOURCE=..\mysys\my_alloc.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_getopt.c
# End Source File
# Begin Source File

SOURCE="..\sql-common\my_time.c"
# End Source File
# Begin Source File

SOURCE="..\sql-common\my_user.c"
# End Source File
# Begin Source File

SOURCE=..\sql\net_serv.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\opt_range.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\opt_sum.cpp
# End Source File
# Begin Source File

SOURCE="..\sql-common\pack.c"
# End Source File
# Begin Source File

SOURCE=..\libmysql\password.c
# End Source File
# Begin Source File

SOURCE=..\sql\procedure.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\protocol.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\records.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\repl_failsafe.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\set_var.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\spatial.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_acl.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_analyse.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_base.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_cache.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_class.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_crypt.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_db.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_delete.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_derived.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_do.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_error.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_handler.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_help.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_insert.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_lex.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_list.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_load.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_locale.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_manager.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_map.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_parse.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_prepare.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_rename.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_repl.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_select.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_show.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_state.c
# End Source File
# Begin Source File

SOURCE=..\sql\sql_string.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_table.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_test.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_udf.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_union.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_update.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_yacc.cpp
# End Source File
# Begin Source File

SOURCE=..\strings\str2int.c
# End Source File
# Begin Source File

SOURCE=..\strings\strcend.c
# End Source File
# Begin Source File

SOURCE=..\strings\strcont.c
# End Source File
# Begin Source File

SOURCE=..\sql\strfunc.cpp
# End Source File
# Begin Source File

SOURCE=..\strings\strinstr.c
# End Source File
# Begin Source File

SOURCE=..\strings\strxnmov.c
# End Source File
# Begin Source File

SOURCE=..\strings\str_alloc.c
# End Source File
# Begin Source File

SOURCE=..\sql\table.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\thr_malloc.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\time.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\tztime.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\uniques.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\unireg.cpp
# End Source File
# Begin Source File

SOURCE=..\vio\vio.c
# End Source File
# Begin Source File

SOURCE=..\vio\viosocket.c
# End Source File
# Begin Source File

SOURCE=..\vio\viossl.c
# End Source File
# Begin Source File

SOURCE=..\vio\viosslfactories.c
# End Source File
# End Target
# End Project
