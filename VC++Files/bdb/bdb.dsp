# Microsoft Developer Studio Project File - Name="bdb" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=bdb - Win32 Max
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "bdb.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "bdb.mak" CFG="bdb - Win32 Max"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "bdb - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "bdb - Win32 Max" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "bdb - Win32 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /G6 /MTd /W3 /Z7 /Od /Gf /I "../bdb/build_win32" /I "../bdb/include" /D "__WIN32__" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_debug\bdb.lib"

!ELSEIF  "$(CFG)" == "bdb - Win32 Max"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "bdb___Win32_Max"
# PROP BASE Intermediate_Dir "bdb___Win32_Max"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "bdb___Win32_Max"
# PROP Intermediate_Dir "bdb___Win32_Max"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MTd /W3 /Z7 /Od /Gf /I "../bdb/build_win32" /I "../bdb/include" /D "__WIN32__" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /FD /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../bdb/build_win32" /I "../bdb/include" /D "NDEBUG" /D "DBUG_OFF" /D "_WINDOWS" /Fo"mysys___Win32_Max/" /Fd"mysys___Win32_Max/" /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\lib_debug\bdb.lib"
# ADD LIB32 /nologo /out:"..\lib_release\bdb.lib"

!ENDIF 

# Begin Target

# Name "bdb - Win32 Debug"
# Name "bdb - Win32 Max"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\btree\bt_compare.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_conv.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_curadj.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_cursor.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_delete.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_method.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_open.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_put.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_rec.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_reclaim.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_recno.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_rsearch.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_search.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_split.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_stat.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_upgrade.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\bt_verify.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\btree\btree_auto.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\crdel_auto.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\crdel_rec.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\cxx\cxx_app.cpp
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\cxx\cxx_except.cpp
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\cxx\cxx_lock.cpp
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\cxx\cxx_log.cpp
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\cxx\cxx_mpool.cpp
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\cxx\cxx_table.cpp
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\cxx\cxx_txn.cpp
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_am.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_auto.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\common\db_byteorder.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_cam.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_conv.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_dispatch.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_dup.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\common\db_err.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\common\db_getlong.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_iface.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_join.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\common\db_log2.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_meta.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_method.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_overflow.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_pr.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_rec.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_reclaim.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_ret.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\env\db_salloc.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\env\db_shash.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_upg.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_upg_opd.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_vrfy.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\db\db_vrfyutil.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\dbm\dbm.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\env\env_method.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\env\env_open.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\env\env_recover.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\env\env_region.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\hash\hash.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\hash\hash_auto.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\hash\hash_conv.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\hash\hash_dup.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\hash\hash_func.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\hash\hash_meta.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\hash\hash_method.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\hash\hash_page.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\hash\hash_rec.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\hash\hash_reclaim.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\hash\hash_stat.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\hash\hash_upgrade.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\hash\hash_verify.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\hsearch\hsearch.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\lock\lock.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\lock\lock_conflict.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\lock\lock_deadlock.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\lock\lock_method.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\lock\lock_region.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\lock\lock_stat.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\lock\lock_util.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\log\log.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\log\log_archive.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\log\log_auto.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\log\log_compare.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\log\log_findckp.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\log\log_get.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\log\log_method.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\log\log_put.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\log\log_rec.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\log\log_register.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\mp\mp_alloc.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\mp\mp_bh.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\mp\mp_fget.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\mp\mp_fopen.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\mp\mp_fput.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\mp\mp_fset.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\mp\mp_method.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\mp\mp_region.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\mp\mp_register.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\mp\mp_stat.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\mp\mp_sync.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\mp\mp_trickle.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\mutex\mut_tas.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\mutex\mutex.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_abs.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\os\os_alloc.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_dir.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_errno.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_fid.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_finit.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\os\os_fsync.c
# End Source File
# Begin Source File

SOURCE=.\os\os_handle.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_map.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\os\os_method.c
# End Source File
# Begin Source File

SOURCE=.\os\os_oflags.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_open.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\os\os_region.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_rename.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\os\os_root.c
# End Source File
# Begin Source File

SOURCE=.\os\os_rpath.c
# End Source File
# Begin Source File

SOURCE=.\os\os_rw.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_seek.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_sleep.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_spin.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\os\os_stat.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\os\os_tmpdir.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_type.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\os\os_unlink.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\qam\qam.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\qam\qam_auto.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\qam\qam_conv.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\qam\qam_files.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\qam\qam_method.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\qam\qam_open.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\qam\qam_rec.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\qam\qam_stat.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\qam\qam_upgrade.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\qam\qam_verify.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\clib\strcasecmp.c
# End Source File
# Begin Source File

SOURCE=.\txn\txn.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\txn\txn_auto.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\txn\txn_rec.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\txn\txn_region.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\common\util_log.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\common\util_sig.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\xa\xa.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\xa\xa_db.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# Begin Source File

SOURCE=.\xa\xa_map.c
# ADD CPP /I "../bdb/build_win32" /I "../bdb/include"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
