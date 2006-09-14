# Microsoft Developer Studio Project File - Name="mysqldmax" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=mysqldmax - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "mysqldmax.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "mysqldmax.mak" CFG="mysqldmax - Win32 Debug"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "mysqldmax - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqldmax - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "mysqldmax - Win32 nt" (based on "Win32 (x86) Console Application")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /D "NDEBUG" /D "DBUG_OFF" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_BERKELEY_DB" /D "HAVE_INNOBASE_DB" /FD /c
# ADD BASE RSC /l 0x416 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\innobase-opt.lib ..\lib_release\libdb32s.lib /nologo /subsystem:console /pdb:none /debug /machine:I386 /nodefaultlib:"LIBC" /out:"../client_release/mysqld-max-opt.exe"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ  /c
# ADD CPP /nologo /G6 /MTd /W3 /Gm /ZI /Od /I "../include" /I "../regex" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_BERKELEY_DB" /D "HAVE_INNOBASE_DB" /FR /FD /c
# ADD BASE RSC /l 0x416 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_debug\dbug.lib ..\lib_debug\isam.lib ..\lib_debug\merge.lib ..\lib_debug\mysys.lib ..\lib_debug\strings.lib ..\lib_debug\regex.lib ..\lib_debug\heap.lib ..\lib_release\innobase-opt.lib ..\lib_release\libdb32s.lib /nologo /subsystem:console /incremental:no /pdb:"debug/mysqld.pdb" /debug /machine:I386 /nodefaultlib:"LIBC" /out:"../client_debug/mysqld-max.exe" /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "nt"
# PROP BASE Intermediate_Dir "nt"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "nt"
# PROP Intermediate_Dir "nt"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../regex" /D "NDEBUG" /D "__NT__" /D "DBUG_OFF" /D "MYSQL_SERVER" /D "_WINDOWS" /D "_CONSOLE" /D "_MBCS" /D "HAVE_BERKELEY_DB" /D "HAVE_INNOBASE_DB" /FD /c
# ADD BASE RSC /l 0x416 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib Wsock32.lib ..\lib_release\isam.lib ..\lib_release\merge.lib ..\lib_release\myisam.lib ..\lib_release\myisammrg.lib ..\lib_release\mysys.lib ..\lib_release\strings.lib ..\lib_release\regex.lib ..\lib_release\heap.lib ..\lib_release\zlib.lib ..\lib_release\innobase-nt.lib ..\lib_release\libdb32s.lib /nologo /subsystem:console /pdb:"NT/mysqld-nt.pdb" /map:"NT/mysqld-nt.map" /machine:I386 /nodefaultlib:"LIBC" /out:"../client_release/mysqld-max-nt.exe"
# SUBTRACT LINK32 /pdb:none

!ENDIF

# Begin Target

# Name "mysqldmax - Win32 Release"
# Name "mysqldmax - Win32 Debug"
# Name "mysqldmax - Win32 nt"
# Begin Source File

SOURCE=.\convert.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\derror.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\field.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\field_conv.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\filesort.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\ha_berkeley.cpp
# End Source File
# Begin Source File

SOURCE=.\ha_heap.cpp
# End Source File
# Begin Source File

SOURCE=.\ha_innobase.cpp
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

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\hash_filo.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\hash_filo.h
# End Source File
# Begin Source File

SOURCE=.\hostname.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\init.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\item.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\item_buff.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\item_cmpfunc.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\item_create.cpp
# End Source File
# Begin Source File

SOURCE=.\item_func.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\item_strfunc.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\item_sum.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\item_timefunc.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\item_uniq.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\key.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\lock.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\log.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\log_event.cpp
# End Source File
# Begin Source File

SOURCE=.\md5.c
# End Source File
# Begin Source File

SOURCE=.\mf_iocache.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\mini_client.cpp
# End Source File
# Begin Source File

SOURCE=.\mini_client_errors.c
# End Source File
# Begin Source File

SOURCE=.\mysqld.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\net_pkg.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\net_serv.cpp
# End Source File
# Begin Source File

SOURCE=.\nt_servc.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\nt_servc.h
# End Source File
# Begin Source File

SOURCE=.\opt_ft.cpp
# End Source File
# Begin Source File

SOURCE=.\opt_range.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\opt_range.h
# End Source File
# Begin Source File

SOURCE=.\opt_sum.cpp
# End Source File
# Begin Source File

SOURCE=.\password.c

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\procedure.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\records.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\slave.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_acl.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_analyse.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_base.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_cache.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_class.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_crypt.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_crypt.h
# End Source File
# Begin Source File

SOURCE=.\sql_db.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_delete.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_insert.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_lex.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_list.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_load.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_locale.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_manager.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_map.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_parse.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_rename.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_repl.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_select.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_show.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_string.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_table.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_test.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_update.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\sql_yacc.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\strfunc.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\table.cpp
# End Source File
# Begin Source File

SOURCE=.\thr_malloc.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\time.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\unireg.cpp

!IF  "$(CFG)" == "mysqldmax - Win32 Release"

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 Debug"

# ADD CPP /G5
# SUBTRACT CPP /YX /Yc /Yu

!ELSEIF  "$(CFG)" == "mysqldmax - Win32 nt"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\violite.c
# End Source File
# End Target
# End Project
