# Microsoft Developer Studio Project File - Name="mysqldemb" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=mysqldemb - WinIA64  pro
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mysqldemb.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mysqldemb.mak" CFG="mysqldemb - WinIA64  pro"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mysqldemb - WinIA64  Release" (based on "Win32 (x86) Static Library")
!MESSAGE "mysqldemb - WinIA64  Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "mysqldemb - WinIA64  classic" (based on "Win32 (x86) Static Library")
!MESSAGE "mysqldemb - WinIA64  pro" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mysqldemb - WinIA64  Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WinIA64" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../regex" /I "../libmysqld" /I "../sql" /I "../bdb/build_WinIA64" /I "../zlib" /D "WinIA64" /D "NDEBUG" /D "_MBCS" /D "_LIB" /D "SIGNAL_WITH_VIO_CLOSE" /D "HAVE_DLOPEN" /D "EMBEDDED_LIBRARY" /D "MYSQL_SERVER" /D "HAVE_INNOBASE_DB" /D "DBUG_OFF" /D "USE_TLS" /D "__WIN__" /D "_IA64_" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x416 /d "NDEBUG"
# ADD RSC /l 0x416 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "mysqldemb - WinIA64  Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WinIA64" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Zi /Od /I "../zlib" /I "../include" /I "../regex" /I "../libmysqld" /I "../sql" /I "../bdb/build_WinIA64" /D "WinIA64" /D "_DEBUG" /D "_MBCS" /D "_LIB" /D "USE_SYMDIR" /D "SIGNAL_WITH_VIO_CLOSE" /D "HAVE_DLOPEN" /D "EMBEDDED_LIBRARY" /D "MYSQL_SERVER" /D "HAVE_INNOBASE_DB" /D "USE_TLS" /D "__WIN__" /D "_IA64_" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /GZ /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x416 /d "_DEBUG"
# ADD RSC /l 0x416 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"../lib_debug/mysqldemb.lib"

!ELSEIF  "$(CFG)" == "mysqldemb - WinIA64  classic"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqldemb___WinIA64 _classic"
# PROP BASE Intermediate_Dir "mysqldemb___WinIA64 _classic"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "classic"
# PROP Intermediate_Dir "classic"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /O2 /I "../include" /I "../regex" /I "../libmysqld" /I "../sql" /I "../zlib" /D "WinIA64" /D "_MBCS" /D "_LIB" /D "USE_SYMDIR" /D "SIGNAL_WITH_VIO_CLOSE" /D "HAVE_DLOPEN" /D "EMBEDDED_LIBRARY" /D "MYSQL_SERVER" /D "DBUG_OFF" /D "USE_TLS" /D "__WIN__" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../regex" /I "../libmysqld" /I "../sql" /I "../zlib" /D "WinIA64" /D "_LIB" /D "SIGNAL_WITH_VIO_CLOSE" /D "HAVE_DLOPEN" /D "EMBEDDED_LIBRARY" /D "MYSQL_SERVER" /D "USE_TLS" /D "__WIN__" /D LICENSE=Commercial /D "DBUG_OFF" /D "_MBCS" /D "NDEBUG" /D "_IA64_" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x416 /d "NDEBUG"
# ADD RSC /l 0x416 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_classic\mysqldemb.lib"

!ELSEIF  "$(CFG)" == "mysqldemb - WinIA64  pro"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysqldemb___WinIA64 _pro"
# PROP BASE Intermediate_Dir "mysqldemb___WinIA64 _pro"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "pro"
# PROP Intermediate_Dir "pro"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /O2 /I "../include" /I "../regex" /I "../libmysqld" /I "../sql" /I "../zlib" /D "WinIA64" /D "_MBCS" /D "_LIB" /D "USE_SYMDIR" /D "SIGNAL_WITH_VIO_CLOSE" /D "HAVE_DLOPEN" /D "EMBEDDED_LIBRARY" /D "MYSQL_SERVER" /D "HAVE_INNOBASE_DB" /D "DBUG_OFF" /D "USE_TLS" /D "__WIN__" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /I "../regex" /I "../libmysqld" /I "../sql" /I "../zlib" /D "WinIA64" /D "_LIB" /D "SIGNAL_WITH_VIO_CLOSE" /D "EMBEDDED_LIBRARY" /D "USE_TLS" /D "__WIN__" /D "USE_SYMDIR" /D "MYSQL_SERVER" /D LICENSE=Commercial /D "_MBCS" /D "HAVE_DLOPEN" /D "HAVE_INNOBASE_DB" /D "DBUG_OFF" /D "NDEBUG" /D "_WINDOWS" /D "_CONSOLE" /D "_IA64_" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /DMYSQL_SERVER_SUFFIX=-pro /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x416 /d "NDEBUG"
# ADD RSC /l 0x416 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_pro\mysqldemb.lib"

!ENDIF 

# Begin Target

# Name "mysqldemb - WinIA64  Release"
# Name "mysqldemb - WinIA64  Debug"
# Name "mysqldemb - WinIA64  classic"
# Name "mysqldemb - WinIA64  pro"
# Begin Source File

SOURCE=..\sql\derror.cpp
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

SOURCE=..\libmysql\get_password.c
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

SOURCE=..\libmysqld\lib_sql.cpp
# End Source File
# Begin Source File

SOURCE=..\libmysqld\libmysqld.c
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

SOURCE=..\sql\net_serv.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\opt_range.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\opt_sum.cpp
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

SOURCE=..\sql\slave.cpp
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

SOURCE=..\sql\sql_insert.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_lex.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\sql_list.cpp
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

SOURCE=..\sql\table.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\thr_malloc.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\time.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\uniques.cpp
# End Source File
# Begin Source File

SOURCE=..\sql\unireg.cpp
# End Source File
# End Target
# End Project
