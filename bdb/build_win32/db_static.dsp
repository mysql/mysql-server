# Microsoft Developer Studio Project File - Name="db_static" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=db_static - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "db_static.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "db_static.mak" CFG="db_static - Win32 Debug Static"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "db_static - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "db_static - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "db_static - Win32 Release Static" (based on\
 "Win32 (x86) Static Library")
!MESSAGE "db_static - Win32 Debug Static" (based on\
 "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe

!IF  "$(CFG)" == "db_static - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "DB_Stati"
# PROP BASE Intermediate_Dir "DB_Stati"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_static"
# PROP Intermediate_Dir "Release_static"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "." /I "../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX"config.h" /FD /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Release_static/libdb32s.lib"

!ELSEIF  "$(CFG)" == "db_static - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "DB_Stat0"
# PROP BASE Intermediate_Dir "DB_Stat0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_static"
# PROP Intermediate_Dir "Debug_static"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /GX /Z7 /Od /I "." /I "../include" /D "CONFIG_TEST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX"config.h" /FD /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"Debug_static/libdb32sd.lib"

!ELSEIF  "$(CFG)" == "db_static - Win32 Release Static"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "DB_Stati"
# PROP BASE Intermediate_Dir "DB_Stati"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_static"
# PROP Intermediate_Dir "Release_static"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /I "." /I "../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX"config.h" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "." /I "../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX"config.h" /FD /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"Release/libdb32s.lib"
# ADD LIB32 /nologo /out:"Release_static/libdb32s.lib"

!ELSEIF  "$(CFG)" == "db_static - Win32 Debug Static"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "DB_Stat0"
# PROP BASE Intermediate_Dir "DB_Stat0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug_static"
# PROP Intermediate_Dir "Debug_static"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /GX /Z7 /Od /I "." /I "../include" /D "CONFIG_TEST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX"config.h" /FD /c
# ADD CPP /nologo /MTd /W3 /GX /Z7 /Od /I "." /I "../include" /D "CONFIG_TEST" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX"config.h" /FD /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"Debug/libdb32sd.lib"
# ADD LIB32 /nologo /out:"Debug_static/libdb32sd.lib"

!ENDIF 

# Begin Target

# Name "db_static - Win32 Release"
# Name "db_static - Win32 Debug"
# Name "db_static - Win32 Release Static"
# Name "db_static - Win32 Debug Static"
# Begin Source File

SOURCE=..\btree\bt_compare.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_conv.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_curadj.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_cursor.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_delete.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_method.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_open.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_put.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_rec.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_reclaim.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_recno.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_rsearch.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_search.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_split.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_stat.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_upgrade.c
# End Source File
# Begin Source File

SOURCE=..\btree\bt_verify.c
# End Source File
# Begin Source File

SOURCE=..\btree\btree_auto.c
# End Source File
# Begin Source File

SOURCE=..\clib\strcasecmp.c
# End Source File
# Begin Source File

SOURCE=..\common\db_byteorder.c
# End Source File
# Begin Source File

SOURCE=..\common\db_err.c
# End Source File
# Begin Source File

SOURCE=..\common\db_getlong.c
# End Source File
# Begin Source File

SOURCE=..\common\db_log2.c
# End Source File
# Begin Source File

SOURCE=..\common\util_log.c
# End Source File
# Begin Source File

SOURCE=..\common\util_sig.c
# End Source File
# Begin Source File

SOURCE=..\cxx\cxx_app.cpp
# End Source File
# Begin Source File

SOURCE=..\cxx\cxx_except.cpp
# End Source File
# Begin Source File

SOURCE=..\cxx\cxx_lock.cpp
# End Source File
# Begin Source File

SOURCE=..\cxx\cxx_log.cpp
# End Source File
# Begin Source File

SOURCE=..\cxx\cxx_mpool.cpp
# End Source File
# Begin Source File

SOURCE=..\cxx\cxx_table.cpp
# End Source File
# Begin Source File

SOURCE=..\cxx\cxx_txn.cpp
# End Source File
# Begin Source File

SOURCE=..\db\crdel_auto.c
# End Source File
# Begin Source File

SOURCE=..\db\crdel_rec.c
# End Source File
# Begin Source File

SOURCE=..\db\db.c
# End Source File
# Begin Source File

SOURCE=..\db\db_am.c
# End Source File
# Begin Source File

SOURCE=..\db\db_auto.c
# End Source File
# Begin Source File

SOURCE=..\db\db_cam.c
# End Source File
# Begin Source File

SOURCE=..\db\db_conv.c
# End Source File
# Begin Source File

SOURCE=..\db\db_dispatch.c
# End Source File
# Begin Source File

SOURCE=..\db\db_dup.c
# End Source File
# Begin Source File

SOURCE=..\db\db_iface.c
# End Source File
# Begin Source File

SOURCE=..\db\db_join.c
# End Source File
# Begin Source File

SOURCE=..\db\db_meta.c
# End Source File
# Begin Source File

SOURCE=..\db\db_method.c
# End Source File
# Begin Source File

SOURCE=..\db\db_overflow.c
# End Source File
# Begin Source File

SOURCE=..\db\db_pr.c
# End Source File
# Begin Source File

SOURCE=..\db\db_rec.c
# End Source File
# Begin Source File

SOURCE=..\db\db_reclaim.c
# End Source File
# Begin Source File

SOURCE=..\db\db_ret.c
# End Source File
# Begin Source File

SOURCE=..\db\db_upg.c
# End Source File
# Begin Source File

SOURCE=..\db\db_upg_opd.c
# End Source File
# Begin Source File

SOURCE=..\db\db_vrfy.c
# End Source File
# Begin Source File

SOURCE=..\db\db_vrfyutil.c
# End Source File
# Begin Source File

SOURCE=..\dbm\dbm.c
# End Source File
# Begin Source File

SOURCE=..\env\db_salloc.c
# End Source File
# Begin Source File

SOURCE=..\env\db_shash.c
# End Source File
# Begin Source File

SOURCE=..\env\env_method.c
# End Source File
# Begin Source File

SOURCE=..\env\env_open.c
# End Source File
# Begin Source File

SOURCE=..\env\env_recover.c
# End Source File
# Begin Source File

SOURCE=..\env\env_region.c
# End Source File
# Begin Source File

SOURCE=..\hash\hash.c
# End Source File
# Begin Source File

SOURCE=..\hash\hash_auto.c
# End Source File
# Begin Source File

SOURCE=..\hash\hash_conv.c
# End Source File
# Begin Source File

SOURCE=..\hash\hash_dup.c
# End Source File
# Begin Source File

SOURCE=..\hash\hash_func.c
# End Source File
# Begin Source File

SOURCE=..\hash\hash_meta.c
# End Source File
# Begin Source File

SOURCE=..\hash\hash_method.c
# End Source File
# Begin Source File

SOURCE=..\hash\hash_page.c
# End Source File
# Begin Source File

SOURCE=..\hash\hash_rec.c
# End Source File
# Begin Source File

SOURCE=..\hash\hash_reclaim.c
# End Source File
# Begin Source File

SOURCE=..\hash\hash_stat.c
# End Source File
# Begin Source File

SOURCE=..\hash\hash_upgrade.c
# End Source File
# Begin Source File

SOURCE=..\hash\hash_verify.c
# End Source File
# Begin Source File

SOURCE=..\hsearch\hsearch.c
# End Source File
# Begin Source File

SOURCE=..\lock\lock.c
# End Source File
# Begin Source File

SOURCE=..\lock\lock_conflict.c
# End Source File
# Begin Source File

SOURCE=..\lock\lock_deadlock.c
# End Source File
# Begin Source File

SOURCE=..\lock\lock_method.c
# End Source File
# Begin Source File

SOURCE=..\lock\lock_region.c
# End Source File
# Begin Source File

SOURCE=..\lock\lock_stat.c
# End Source File
# Begin Source File

SOURCE=..\lock\lock_util.c
# End Source File
# Begin Source File

SOURCE=..\log\log.c
# End Source File
# Begin Source File

SOURCE=..\log\log_archive.c
# End Source File
# Begin Source File

SOURCE=..\log\log_auto.c
# End Source File
# Begin Source File

SOURCE=..\log\log_compare.c
# End Source File
# Begin Source File

SOURCE=..\log\log_findckp.c
# End Source File
# Begin Source File

SOURCE=..\log\log_get.c
# End Source File
# Begin Source File

SOURCE=..\log\log_method.c
# End Source File
# Begin Source File

SOURCE=..\log\log_put.c
# End Source File
# Begin Source File

SOURCE=..\log\log_rec.c
# End Source File
# Begin Source File

SOURCE=..\log\log_register.c
# End Source File
# Begin Source File

SOURCE=..\mp\mp_alloc.c
# End Source File
# Begin Source File

SOURCE=..\mp\mp_bh.c
# End Source File
# Begin Source File

SOURCE=..\mp\mp_fget.c
# End Source File
# Begin Source File

SOURCE=..\mp\mp_fopen.c
# End Source File
# Begin Source File

SOURCE=..\mp\mp_fput.c
# End Source File
# Begin Source File

SOURCE=..\mp\mp_fset.c
# End Source File
# Begin Source File

SOURCE=..\mp\mp_method.c
# End Source File
# Begin Source File

SOURCE=..\mp\mp_region.c
# End Source File
# Begin Source File

SOURCE=..\mp\mp_register.c
# End Source File
# Begin Source File

SOURCE=..\mp\mp_stat.c
# End Source File
# Begin Source File

SOURCE=..\mp\mp_sync.c
# End Source File
# Begin Source File

SOURCE=..\mp\mp_trickle.c
# End Source File
# Begin Source File

SOURCE=..\mutex\mut_tas.c
# End Source File
# Begin Source File

SOURCE=..\mutex\mutex.c
# End Source File
# Begin Source File

SOURCE=..\os\os_alloc.c
# End Source File
# Begin Source File

SOURCE=..\os\os_fsync.c
# End Source File
# Begin Source File

SOURCE=..\os\os_handle.c
# End Source File
# Begin Source File

SOURCE=..\os\os_method.c
# End Source File
# Begin Source File

SOURCE=..\os\os_oflags.c
# End Source File
# Begin Source File

SOURCE=..\os\os_region.c
# End Source File
# Begin Source File

SOURCE=..\os\os_root.c
# End Source File
# Begin Source File

SOURCE=..\os\os_rpath.c
# End Source File
# Begin Source File

SOURCE=..\os\os_rw.c
# End Source File
# Begin Source File

SOURCE=..\os\os_stat.c
# End Source File
# Begin Source File

SOURCE=..\os\os_tmpdir.c
# End Source File
# Begin Source File

SOURCE=..\os\os_unlink.c
# End Source File
# Begin Source File

SOURCE=..\os_win32\os_abs.c
# End Source File
# Begin Source File

SOURCE=..\os_win32\os_dir.c
# End Source File
# Begin Source File

SOURCE=..\os_win32\os_errno.c
# End Source File
# Begin Source File

SOURCE=..\os_win32\os_fid.c
# End Source File
# Begin Source File

SOURCE=..\os_win32\os_finit.c
# End Source File
# Begin Source File

SOURCE=..\os_win32\os_map.c
# End Source File
# Begin Source File

SOURCE=..\os_win32\os_open.c
# End Source File
# Begin Source File

SOURCE=..\os_win32\os_rename.c
# End Source File
# Begin Source File

SOURCE=..\os_win32\os_seek.c
# End Source File
# Begin Source File

SOURCE=..\os_win32\os_sleep.c
# End Source File
# Begin Source File

SOURCE=..\os_win32\os_spin.c
# End Source File
# Begin Source File

SOURCE=..\os_win32\os_type.c
# End Source File
# Begin Source File

SOURCE=..\qam\qam.c
# End Source File
# Begin Source File

SOURCE=..\qam\qam_auto.c
# End Source File
# Begin Source File

SOURCE=..\qam\qam_conv.c
# End Source File
# Begin Source File

SOURCE=..\qam\qam_files.c
# End Source File
# Begin Source File

SOURCE=..\qam\qam_method.c
# End Source File
# Begin Source File

SOURCE=..\qam\qam_open.c
# End Source File
# Begin Source File

SOURCE=..\qam\qam_rec.c
# End Source File
# Begin Source File

SOURCE=..\qam\qam_stat.c
# End Source File
# Begin Source File

SOURCE=..\qam\qam_upgrade.c
# End Source File
# Begin Source File

SOURCE=..\qam\qam_verify.c
# End Source File
# Begin Source File

SOURCE=..\txn\txn.c
# End Source File
# Begin Source File

SOURCE=..\txn\txn_auto.c
# End Source File
# Begin Source File

SOURCE=..\txn\txn_rec.c
# End Source File
# Begin Source File

SOURCE=..\txn\txn_region.c
# End Source File
# Begin Source File

SOURCE=..\xa\xa.c
# End Source File
# Begin Source File

SOURCE=..\xa\xa_db.c
# End Source File
# Begin Source File

SOURCE=..\xa\xa_map.c
# End Source File
# End Target
# End Project
