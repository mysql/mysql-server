# Microsoft Developer Studio Project File - Name="mysqlclient" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=mysqlclient - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "mysqlclient.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "mysqlclient.mak" CFG="mysqlclient - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "mysqlclient - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "mysqlclient - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "mysqlclient - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "release"
# PROP Intermediate_Dir "release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../" /D "NDEBUG" /D "DBUG_OFF" /D "_WINDOWS" /D "USE_TLS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_release\mysqlclient.lib"

!ELSEIF  "$(CFG)" == "mysqlclient - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /G6 /MTd /W3 /ZI /Od /I "../include" /I "../" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /D "USE_TLS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_debug\mysqlclient.lib"

!ENDIF 

# Begin Target

# Name "mysqlclient - Win32 Release"
# Name "mysqlclient - Win32 Debug"
# Begin Source File

SOURCE=..\mysys\array.c
# End Source File
# Begin Source File

SOURCE=..\strings\bchange.c
# End Source File
# Begin Source File

SOURCE=..\strings\bmove.c
# End Source File
# Begin Source File

SOURCE=..\strings\bmove_upp.c
# End Source File
# Begin Source File

SOURCE=..\mysys\charset.c
# End Source File
# Begin Source File

SOURCE=.\completion_hash.cpp
# End Source File
# Begin Source File

SOURCE=.\completion_hash.h
# End Source File
# Begin Source File

SOURCE="..\strings\ctype-big5.c"
# End Source File
# Begin Source File

SOURCE="..\strings\ctype-czech.c"
# End Source File
# Begin Source File

SOURCE="..\strings\ctype-euc_kr.c"
# End Source File
# Begin Source File

SOURCE="..\strings\ctype-gb2312.c"
# End Source File
# Begin Source File

SOURCE="..\strings\ctype-gbk.c"
# End Source File
# Begin Source File

SOURCE="..\strings\ctype-latin1_de.c"
# End Source File
# Begin Source File

SOURCE="..\strings\ctype-sjis.c"
# End Source File
# Begin Source File

SOURCE="..\strings\ctype-tis620.c"
# End Source File
# Begin Source File

SOURCE="..\strings\ctype-ujis.c"
# End Source File
# Begin Source File

SOURCE=..\strings\ctype.c
# End Source File
# Begin Source File

SOURCE=..\dbug\dbug.c
# End Source File
# Begin Source File

SOURCE=..\mysys\default.c
# End Source File
# Begin Source File

SOURCE=..\libmysql\errmsg.c
# End Source File
# Begin Source File

SOURCE=..\mysys\errors.c
# End Source File
# Begin Source File

SOURCE=..\libmysql\get_password.c
# End Source File
# Begin Source File

SOURCE=..\mysys\getopt.c
# End Source File
# Begin Source File

SOURCE=..\mysys\getopt1.c
# End Source File
# Begin Source File

SOURCE=..\mysys\getvar.c
# End Source File
# Begin Source File

SOURCE=..\strings\int2str.c
# End Source File
# Begin Source File

SOURCE=..\strings\is_prefix.c
# End Source File
# Begin Source File

SOURCE=..\libmysql\libmysql.c
# End Source File
# Begin Source File

SOURCE=..\mysys\list.c
# End Source File
# Begin Source File

SOURCE=..\strings\llstr.c
# End Source File
# Begin Source File

SOURCE=..\strings\longlong2str.c
# End Source File
# Begin Source File

SOURCE=..\mysys\mf_cache.c
# End Source File
# Begin Source File

SOURCE=..\mysys\mf_casecnv.c
# End Source File
# Begin Source File

SOURCE=..\mysys\mf_dirname.c
# End Source File
# Begin Source File

SOURCE=..\mysys\mf_fn_ext.c
# End Source File
# Begin Source File

SOURCE=..\mysys\mf_format.c
# End Source File
# Begin Source File

SOURCE=..\mysys\mf_iocache.c
# End Source File
# Begin Source File

SOURCE=..\mysys\mf_loadpath.c
# End Source File
# Begin Source File

SOURCE=..\mysys\mf_pack.c
# End Source File
# Begin Source File

SOURCE=..\mysys\mf_path.c
# End Source File
# Begin Source File

SOURCE=..\mysys\mf_tempfile.c
# End Source File
# Begin Source File

SOURCE=..\mysys\mf_unixpath.c
# End Source File
# Begin Source File

SOURCE=..\mysys\mf_wcomp.c
# End Source File
# Begin Source File

SOURCE=..\mysys\mulalloc.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_alloc.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_compress.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_create.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_delete.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_div.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_error.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_fopen.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_fstream.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_getwd.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_init.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_lib.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_malloc.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_messnc.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_net.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_once.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_open.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_pread.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_pthread.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_read.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_realloc.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_rename.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_seek.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_static.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_symlink.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_symlink2.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_tempnam.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_thr_init.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_wincond.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_winthread.c
# End Source File
# Begin Source File

SOURCE=..\mysys\my_write.c
# End Source File
# Begin Source File

SOURCE=.\mysys_priv.h
# End Source File
# Begin Source File

SOURCE=..\libmysql\net.c
# End Source File
# Begin Source File

SOURCE=..\libmysql\password.c
# End Source File
# Begin Source File

SOURCE=.\readline.cpp
# End Source File
# Begin Source File

SOURCE=..\mysys\safemalloc.c
# End Source File
# Begin Source File

SOURCE=.\select_test.c
# End Source File
# Begin Source File

SOURCE=.\sql_string.cpp
# End Source File
# Begin Source File

SOURCE=.\sql_string.h
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

SOURCE=..\strings\strend.c
# End Source File
# Begin Source File

SOURCE=..\strings\strfill.c
# End Source File
# Begin Source File

SOURCE=..\mysys\string.c
# End Source File
# Begin Source File

SOURCE=..\strings\strinstr.c
# End Source File
# Begin Source File

SOURCE=..\strings\strmake.c
# End Source File
# Begin Source File

SOURCE=..\strings\strmov.c
# End Source File
# Begin Source File

SOURCE=..\strings\strnlen.c
# End Source File
# Begin Source File

SOURCE=..\strings\strnmov.c
# End Source File
# Begin Source File

SOURCE=..\strings\strtoll.c
# End Source File
# Begin Source File

SOURCE=..\strings\strtoull.c
# End Source File
# Begin Source File

SOURCE=..\strings\strxmov.c
# End Source File
# Begin Source File

SOURCE=..\mysys\thr_mutex.c
# End Source File
# Begin Source File

SOURCE=..\mysys\typelib.c
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
