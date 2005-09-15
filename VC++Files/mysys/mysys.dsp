# Microsoft Developer Studio Project File - Name="mysys" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=mysys - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "mysys.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "mysys.mak" CFG="mysys - Win32 Debug"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "mysys - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "mysys - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "mysys - Win32 Max" (based on "Win32 (x86) Static Library")
!MESSAGE "mysys - Win32 TLS_DEBUG" (based on "Win32 (x86) Static Library")
!MESSAGE "mysys - Win32 TLS" (based on "Win32 (x86) Static Library")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl6.exe
RSC=rc.exe

!IF  "$(CFG)" == "mysys - Win32 Release"

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
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../zlib" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /FD /c
# SUBTRACT CPP /WX /Fr /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=xilink6.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_release\mysys.lib"

!ELSEIF  "$(CFG)" == "mysys - Win32 Debug"

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
# ADD CPP /nologo /G6 /MTd /W3 /Z7 /Od /I "../include" /I "../zlib" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /D "USE_SYMDIR" /FD /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=xilink6.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_debug\mysys.lib"

!ELSEIF  "$(CFG)" == "mysys - Win32 Max"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysys___Win32_Max"
# PROP BASE Intermediate_Dir "mysys___Win32_Max"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "max"
# PROP Intermediate_Dir "max"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /D "NDEBUG" /D "DBUG_OFF" /D "_WINDOWS" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../zlib" /D "USE_SYMDIR" /D "NDEBUG" /D "DBUG_OFF" /D "_WINDOWS" /D MYSQL_SERVER_SUFFIX=-max /FD  /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=xilink6.exe -lib
# ADD BASE LIB32 /nologo /out:"..\lib_release\mysys.lib"
# ADD LIB32 /nologo /out:"..\lib_release\mysys-max.lib"

!ELSEIF  "$(CFG)" == "mysys - Win32 TLS_DEBUG"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "mysys___Win32_TLS_DEBUG"
# PROP BASE Intermediate_Dir "mysys___Win32_TLS_DEBUG"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "mysys___Win32_TLS_DEBUG"
# PROP Intermediate_Dir "mysys___Win32_TLS_DEBUG"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MTd /W3 /Z7 /Od /I "../include" /I "../zlib" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /D "USE_SYMDIR" /FD /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /G6 /MTd /W3 /Z7 /Od /I "../include" /I "../zlib" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /D "USE_SYMDIR" /D "USE_TLS" /FD /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\lib_debug\mysys_tls.lib"
# ADD LIB32 /nologo /out:"..\lib_debug\mysys_tls.lib"

!ELSEIF  "$(CFG)" == "mysys - Win32 TLS"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "mysys___Win32_TLS"
# PROP BASE Intermediate_Dir "mysys___Win32_TLS"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "mysys___Win32_TLS"
# PROP Intermediate_Dir "mysys___Win32_TLS"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../zlib" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /I "../zlib" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /D "USE_TLS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\lib_release\mysys_tls.lib"
# ADD LIB32 /nologo /out:"..\lib_release\mysys_tls.lib"

!ENDIF

# Begin Target

# Name "mysys - Win32 Release"
# Name "mysys - Win32 Debug"
# Name "mysys - Win32 Max"
# Name "mysys - Win32 TLS_DEBUG"
# Name "mysys - Win32 TLS"
# Begin Source File

SOURCE=.\array.c

!IF  "$(CFG)" == "mysys - Win32 Release"

!ELSEIF  "$(CFG)" == "mysys - Win32 Debug"

# SUBTRACT CPP /Fr

!ELSEIF  "$(CFG)" == "mysys - Win32 Max"


!ELSEIF  "$(CFG)" == "mysys - Win32 TLS_DEBUG"

# ADD BASE CPP /FR
# ADD CPP /FR

!ELSEIF  "$(CFG)" == "mysys - Win32 TLS"

!ENDIF

# End Source File
 
# Begin Source File
SOURCE=".\my_access.c"
# End Source File
 
# Begin Source File

SOURCE=".\charset-def.c"
# End Source File
# Begin Source File

SOURCE=.\charset.c
# End Source File
# Begin Source File

SOURCE=.\checksum.c
# End Source File
# Begin Source File

SOURCE=.\default.c
# End Source File
# Begin Source File

SOURCE=.\errors.c
# End Source File
# Begin Source File

SOURCE=.\hash.c
# End Source File
# Begin Source File

SOURCE=.\list.c
# End Source File
# Begin Source File

SOURCE=.\md5.c
# End Source File
# Begin Source File

SOURCE=.\mf_brkhant.c
# End Source File
# Begin Source File

SOURCE=.\mf_cache.c
# End Source File
# Begin Source File

SOURCE=.\mf_dirname.c
# End Source File
# Begin Source File

SOURCE=.\mf_fn_ext.c
# End Source File
# Begin Source File

SOURCE=.\mf_format.c
# End Source File
# Begin Source File

SOURCE=.\mf_getdate.c
# End Source File
# Begin Source File

SOURCE=.\mf_iocache.c
# End Source File
# Begin Source File

SOURCE=.\mf_iocache2.c
# End Source File
# Begin Source File

SOURCE=.\mf_keycache.c
# End Source File
# Begin Source File

SOURCE=.\mf_keycaches.c
# End Source File
# Begin Source File

SOURCE=.\mf_loadpath.c
# End Source File
# Begin Source File

SOURCE=.\mf_pack.c
# End Source File
# Begin Source File

SOURCE=.\mf_path.c
# End Source File
# Begin Source File

SOURCE=.\mf_qsort.c
# End Source File
# Begin Source File

SOURCE=.\mf_qsort2.c
# End Source File
# Begin Source File

SOURCE=.\mf_radix.c
# End Source File
# Begin Source File

SOURCE=.\mf_same.c
# End Source File
# Begin Source File

SOURCE=.\mf_sort.c
# End Source File
# Begin Source File

SOURCE=.\mf_soundex.c
# End Source File
# Begin Source File

SOURCE=.\mf_strip.c
# End Source File
# Begin Source File

SOURCE=.\mf_tempdir.c
# End Source File
# Begin Source File

SOURCE=.\mf_tempfile.c
# End Source File
# Begin Source File

SOURCE=.\mf_wcomp.c
# End Source File
# Begin Source File

SOURCE=.\mf_wfile.c
# End Source File
# Begin Source File

SOURCE=.\mulalloc.c
# End Source File
# Begin Source File

SOURCE=.\my_aes.c
# End Source File
# Begin Source File

SOURCE=.\my_alarm.c
# End Source File
# Begin Source File

SOURCE=.\my_alloc.c
# End Source File
# Begin Source File

SOURCE=.\my_append.c
# End Source File
# Begin Source File

SOURCE=.\my_bit.c
# End Source File
# Begin Source File

SOURCE=.\my_bitmap.c
# End Source File
# Begin Source File

SOURCE=.\my_chsize.c
# End Source File
# Begin Source File

SOURCE=.\my_clock.c
# End Source File
# Begin Source File

SOURCE=.\my_compress.c
# End Source File
# Begin Source File

SOURCE=.\my_conio.c
# End Source File
# Begin Source File

SOURCE=.\my_copy.c
# End Source File
# Begin Source File

SOURCE=.\my_crc32.c
# End Source File
# Begin Source File

SOURCE=.\my_create.c
# End Source File
# Begin Source File

SOURCE=.\my_delete.c
# End Source File
# Begin Source File

SOURCE=.\my_div.c
# End Source File
# Begin Source File

SOURCE=.\my_error.c
# End Source File
# Begin Source File

SOURCE=.\my_fopen.c
# End Source File
# Begin Source File

SOURCE=.\my_fstream.c
# End Source File
# Begin Source File

SOURCE=.\my_gethostbyname.c
# End Source File
# Begin Source File

SOURCE=.\my_gethwaddr.c
# End Source File
# Begin Source File

SOURCE=.\my_getopt.c
# End Source File
# Begin Source File

SOURCE=.\my_getsystime.c
# End Source File
# Begin Source File

SOURCE=.\my_getwd.c
# End Source File
# Begin Source File

SOURCE=.\my_handler.c
# End Source File
# Begin Source File

SOURCE=.\my_init.c
# End Source File
# Begin Source File

SOURCE=.\my_lib.c
# End Source File
# Begin Source File

SOURCE=.\my_lock.c
# End Source File
# Begin Source File

SOURCE=.\my_lockmem.c
# End Source File
# Begin Source File

SOURCE=.\my_lread.c
# End Source File
# Begin Source File

SOURCE=.\my_lwrite.c
# End Source File
# Begin Source File

SOURCE=.\my_malloc.c
# End Source File
# Begin Source File

SOURCE=.\my_messnc.c
# End Source File
# Begin Source File

SOURCE=.\my_mkdir.c
# End Source File
# Begin Source File

SOURCE=.\my_net.c
# End Source File
# Begin Source File

SOURCE=.\my_once.c
# End Source File
# Begin Source File

SOURCE=.\my_open.c
# End Source File
# Begin Source File

SOURCE=.\my_file.c
# End Source File
# Begin Source File

SOURCE=.\my_pread.c
# End Source File
# Begin Source File

SOURCE=.\my_pthread.c
# End Source File
# Begin Source File

SOURCE=.\my_quick.c
# End Source File
# Begin Source File

SOURCE=.\my_read.c
# End Source File
# Begin Source File

SOURCE=.\my_realloc.c
# End Source File
# Begin Source File

SOURCE=.\my_redel.c
# End Source File
# Begin Source File

SOURCE=.\my_rename.c
# End Source File
# Begin Source File

SOURCE=.\my_seek.c
# End Source File
# Begin Source File

SOURCE=.\my_sleep.c
# End Source File
# Begin Source File

SOURCE=.\my_static.c
# End Source File
# Begin Source File

SOURCE=.\my_static.h
# End Source File
# Begin Source File

SOURCE=.\my_symlink.c
# End Source File
# Begin Source File

SOURCE=.\my_symlink2.c
# End Source File
# Begin Source File

SOURCE=.\my_sync.c
# End Source File
# Begin Source File

SOURCE=.\my_tempnam.c
# End Source File
# Begin Source File

SOURCE=.\my_thr_init.c
# End Source File
# Begin Source File

SOURCE=.\my_wincond.c
# End Source File
# Begin Source File

SOURCE=.\my_windac.c
# End Source File
# Begin Source File


SOURCE=.\my_winsem.c
# End Source File
# Begin Source File

SOURCE=.\my_winthread.c
# End Source File
# Begin Source File

SOURCE=.\my_write.c
# End Source File
# Begin Source File

SOURCE=.\mysys_priv.h
# End Source File
# Begin Source File

SOURCE=.\ptr_cmp.c
# End Source File
# Begin Source File

SOURCE=.\queues.c
# End Source File
# Begin Source File

SOURCE=.\raid.cpp
# End Source File
# Begin Source File

SOURCE=.\rijndael.c
# End Source File
# Begin Source File

SOURCE=.\safemalloc.c
# End Source File
# Begin Source File

SOURCE=.\sha1.c
# End Source File
# Begin Source File

SOURCE=.\string.c
# End Source File
# Begin Source File

SOURCE=.\thr_alarm.c
# End Source File
# Begin Source File

SOURCE=.\thr_lock.c

!IF  "$(CFG)" == "mysys - Win32 Release"

!ELSEIF  "$(CFG)" == "mysys - Win32 Debug"

# ADD CPP /D "EXTRA_DEBUG"

!ELSEIF  "$(CFG)" == "mysys - Win32 Max"

!ELSEIF  "$(CFG)" == "mysys - Win32 TLS_DEBUG"

# ADD BASE CPP /D "EXTRA_DEBUG"
# ADD CPP /D "EXTRA_DEBUG"

!ELSEIF  "$(CFG)" == "mysys - Win32 TLS"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\thr_mutex.c
# End Source File
# Begin Source File

SOURCE=.\thr_rwlock.c
# End Source File
# Begin Source File

SOURCE=.\tree.c
# End Source File
# Begin Source File

SOURCE=.\typelib.c
# End Source File
# End Target
# End Project
