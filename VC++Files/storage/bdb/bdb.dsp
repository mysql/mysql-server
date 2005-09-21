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
CPP=xicl6.exe
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
# ADD CPP /nologo /G6 /MTd /W3 /Z7 /Od /Gf /I "../bdb/build_win32" /I "../bdb" /I "../bdb/dbinc" /D "__WIN32__" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=xilink6.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib_debug\bdb.lib"

!ELSEIF  "$(CFG)" == "bdb - Win32 Max"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "max"
# PROP BASE Intermediate_Dir "max"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "max"
# PROP Intermediate_Dir "max"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MTd /W3 /Z7 /Od /Gf /I "../bdb/build_win32" /I "../bdb/include" /D "__WIN32__" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /FD /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../bdb/build_win32" /I "../bdb" /D "NDEBUG" /D "DBUG_OFF" /D "_WINDOWS" /D MYSQL_SERVER_SUFFIX=-max /Fo"max/" /Fd"max/" /FD /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=xilink6.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\lib_debug\bdb.lib"
# ADD LIB32 /nologo /out:"..\..\lib_release\bdb.lib"

!ENDIF

# Begin Target

# Name "bdb - Win32 Debug"
# Name "bdb - Win32 Max"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\btree\bt_compare.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_conv.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_curadj.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_cursor.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_delete.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_method.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_open.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_put.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_rec.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_reclaim.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_recno.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_rsearch.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_search.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_split.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_stat.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_upgrade.c
# End Source File
# Begin Source File

SOURCE=.\btree\bt_verify.c
# End Source File
# Begin Source File

SOURCE=.\btree\btree_auto.c
# End Source File
# Begin Source File

SOURCE=.\crypto\aes_method.c
# End Source File
# Begin Source File

SOURCE=.\crypto\crypto.c
# End Source File
# Begin Source File

SOURCE=.\crypto\mersenne\mt19937db.c
# End Source File
# Begin Source File

SOURCE=.\crypto\rijndael\rijndael-alg-fst.c
# End Source File
# Begin Source File

SOURCE=.\crypto\rijndael\rijndael-api-fst.c
# End Source File
# Begin Source File

SOURCE=.\db\crdel_auto.c
# End Source File
# Begin Source File

SOURCE=.\db\crdel_rec.c
# End Source File
# Begin Source File

SOURCE=.\db\db.c
# End Source File
# Begin Source File

SOURCE=.\db\db_am.c
# End Source File
# Begin Source File

SOURCE=.\db\db_auto.c
# End Source File
# Begin Source File

SOURCE=.\common\db_byteorder.c
# End Source File
# Begin Source File

SOURCE=.\db\db_cam.c
# End Source File
# Begin Source File

SOURCE=.\db\db_conv.c
# End Source File
# Begin Source File

SOURCE=.\db\db_dispatch.c
# End Source File
# Begin Source File

SOURCE=.\db\db_dup.c
# End Source File
# Begin Source File

SOURCE=.\common\db_err.c
# End Source File
# Begin Source File

SOURCE=.\common\db_getlong.c
# End Source File
# Begin Source File

SOURCE=.\common\db_idspace.c
# End Source File
# Begin Source File

SOURCE=.\db\db_iface.c
# End Source File
# Begin Source File

SOURCE=.\db\db_join.c
# End Source File
# Begin Source File

SOURCE=.\common\db_log2.c
# End Source File
# Begin Source File

SOURCE=.\db\db_meta.c
# End Source File
# Begin Source File

SOURCE=.\db\db_method.c
# End Source File
# Begin Source File

SOURCE=.\db\db_open.c
# End Source File
# Begin Source File

SOURCE=.\db\db_overflow.c
# End Source File
# Begin Source File

SOURCE=.\db\db_ovfl_vrfy.c
# End Source File
# Begin Source File

SOURCE=.\db\db_pr.c
# End Source File
# Begin Source File

SOURCE=.\db\db_rec.c
# End Source File
# Begin Source File

SOURCE=.\db\db_reclaim.c
# End Source File
# Begin Source File

SOURCE=.\db\db_remove.c
# End Source File
# Begin Source File

SOURCE=.\db\db_rename.c
# End Source File
# Begin Source File

SOURCE=.\db\db_ret.c
# End Source File
# Begin Source File

SOURCE=.\db\db_setid.c
# End Source File
# Begin Source File

SOURCE=.\db\db_setlsn.c
# End Source File
# Begin Source File

SOURCE=.\db\db_stati.c
# End Source File
# Begin Source File

SOURCE=.\env\db_salloc.c
# End Source File
# Begin Source File

SOURCE=.\env\db_shash.c
# End Source File
# Begin Source File

SOURCE=.\db\db_truncate.c
# End Source File
# Begin Source File

SOURCE=.\db\db_upg.c
# End Source File
# Begin Source File

SOURCE=.\db\db_upg_opd.c
# End Source File
# Begin Source File

SOURCE=.\db\db_vrfy.c
# End Source File
# Begin Source File

SOURCE=.\db\db_vrfyutil.c
# End Source File
# Begin Source File

SOURCE=.\dbm\dbm.c
# End Source File
# Begin Source File

SOURCE=.\dbreg\dbreg.c
# End Source File
# Begin Source File

SOURCE=.\dbreg\dbreg_auto.c
# End Source File
# Begin Source File

SOURCE=.\dbreg\dbreg_rec.c
# End Source File
# Begin Source File

SOURCE=.\dbreg\dbreg_stat.c
# End Source File
# Begin Source File

SOURCE=.\dbreg\dbreg_util.c
# End Source File
# Begin Source File

SOURCE=.\env\env_file.c
# End Source File
# Begin Source File

SOURCE=.\env\env_method.c
# End Source File
# Begin Source File

SOURCE=.\env\env_open.c
# End Source File
# Begin Source File

SOURCE=.\env\env_recover.c
# End Source File
# Begin Source File

SOURCE=.\env\env_region.c
# End Source File
# Begin Source File

SOURCE=.\env\env_stat.c
# End Source File
# Begin Source File

SOURCE=.\fileops\fileops_auto.c
# End Source File
# Begin Source File

SOURCE=.\fileops\fop_basic.c
# End Source File
# Begin Source File

SOURCE=.\fileops\fop_rec.c
# End Source File
# Begin Source File

SOURCE=.\fileops\fop_util.c
# End Source File
# Begin Source File

SOURCE=.\hash\hash.c
# End Source File
# Begin Source File

SOURCE=.\hash\hash_auto.c
# End Source File
# Begin Source File

SOURCE=.\hash\hash_conv.c
# End Source File
# Begin Source File

SOURCE=.\hash\hash_dup.c
# End Source File
# Begin Source File

SOURCE=.\hash\hash_func.c
# End Source File
# Begin Source File

SOURCE=.\hash\hash_meta.c
# End Source File
# Begin Source File

SOURCE=.\hash\hash_method.c
# End Source File
# Begin Source File

SOURCE=.\hash\hash_open.c
# End Source File
# Begin Source File

SOURCE=.\hash\hash_page.c
# End Source File
# Begin Source File

SOURCE=.\hash\hash_rec.c
# End Source File
# Begin Source File

SOURCE=.\hash\hash_reclaim.c
# End Source File
# Begin Source File

SOURCE=.\hash\hash_stat.c
# End Source File
# Begin Source File

SOURCE=.\hash\hash_upgrade.c
# End Source File
# Begin Source File

SOURCE=.\hash\hash_verify.c
# End Source File
# Begin Source File

SOURCE=.\hmac\hmac.c
# End Source File
# Begin Source File

SOURCE=.\hsearch\hsearch.c
# End Source File
# Begin Source File

SOURCE=.\lock\lock.c
# End Source File
# Begin Source File

SOURCE=.\lock\lock_deadlock.c
# End Source File
# Begin Source File

SOURCE=.\lock\lock_id.c
# End Source File
# Begin Source File

SOURCE=.\lock\lock_list.c
# End Source File
# Begin Source File

SOURCE=.\lock\lock_method.c
# End Source File
# Begin Source File

SOURCE=.\lock\lock_region.c
# End Source File
# Begin Source File

SOURCE=.\lock\lock_stat.c
# End Source File
# Begin Source File

SOURCE=.\lock\lock_timer.c
# End Source File
# Begin Source File

SOURCE=.\lock\lock_util.c
# End Source File
# Begin Source File

SOURCE=.\log\log.c
# End Source File
# Begin Source File

SOURCE=.\log\log_archive.c
# End Source File
# Begin Source File

SOURCE=.\log\log_compare.c
# End Source File
# Begin Source File

SOURCE=.\log\log_get.c
# End Source File
# Begin Source File

SOURCE=.\log\log_method.c
# End Source File
# Begin Source File

SOURCE=.\log\log_put.c
# End Source File
# Begin Source File

SOURCE=.\log\log_stat.c
# End Source File
# Begin Source File

SOURCE=.\mp\mp_alloc.c
# End Source File
# Begin Source File

SOURCE=.\mp\mp_bh.c
# End Source File
# Begin Source File

SOURCE=.\mp\mp_fget.c
# End Source File
# Begin Source File

SOURCE=.\mp\mp_fmethod.c
# End Source File
# Begin Source File

SOURCE=.\mp\mp_fopen.c
# End Source File
# Begin Source File

SOURCE=.\mp\mp_fput.c
# End Source File
# Begin Source File

SOURCE=.\mp\mp_fset.c
# End Source File
# Begin Source File

SOURCE=.\mp\mp_method.c
# End Source File
# Begin Source File

SOURCE=.\mp\mp_region.c
# End Source File
# Begin Source File

SOURCE=.\mp\mp_register.c
# End Source File
# Begin Source File

SOURCE=.\mp\mp_stat.c
# End Source File
# Begin Source File

SOURCE=.\mp\mp_sync.c
# End Source File
# Begin Source File

SOURCE=.\mp\mp_trickle.c
# End Source File
# Begin Source File

SOURCE=.\mutex\mut_tas.c
# End Source File
# Begin Source File

SOURCE=.\mutex\mut_win32.c
# End Source File
# Begin Source File

SOURCE=.\mutex\mutex.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_abs.c
# End Source File
# Begin Source File

SOURCE=.\os\os_alloc.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_clock.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_config.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_dir.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_errno.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_fid.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_fsync.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_handle.c
# End Source File
# Begin Source File

SOURCE=.\os\os_id.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_map.c
# End Source File
# Begin Source File

SOURCE=.\os\os_method.c
# End Source File
# Begin Source File

SOURCE=.\os\os_oflags.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_open.c
# End Source File
# Begin Source File

SOURCE=.\os\os_region.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_rename.c
# End Source File
# Begin Source File

SOURCE=.\os\os_root.c
# End Source File
# Begin Source File

SOURCE=.\os\os_rpath.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_rw.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_seek.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_sleep.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_spin.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_stat.c
# End Source File
# Begin Source File

SOURCE=.\os\os_tmpdir.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_truncate.c
# End Source File
# Begin Source File

SOURCE=.\os_win32\os_type.c
# End Source File
# Begin Source File

SOURCE=.\os\os_unlink.c
# End Source File
# Begin Source File

SOURCE=.\qam\qam.c
# End Source File
# Begin Source File

SOURCE=.\qam\qam_auto.c
# End Source File
# Begin Source File

SOURCE=.\qam\qam_conv.c
# End Source File
# Begin Source File

SOURCE=.\qam\qam_files.c
# End Source File
# Begin Source File

SOURCE=.\qam\qam_method.c
# End Source File
# Begin Source File

SOURCE=.\qam\qam_open.c
# End Source File
# Begin Source File

SOURCE=.\qam\qam_rec.c
# End Source File
# Begin Source File

SOURCE=.\qam\qam_stat.c
# End Source File
# Begin Source File

SOURCE=.\qam\qam_upgrade.c
# End Source File
# Begin Source File

SOURCE=.\qam\qam_verify.c
# End Source File
# Begin Source File

SOURCE=.\rep\rep_auto.c
# End Source File
# Begin Source File

SOURCE=.\rep\rep_backup.c
# End Source File
# Begin Source File

SOURCE=.\rep\rep_method.c
# End Source File
# Begin Source File

SOURCE=.\rep\rep_record.c
# End Source File
# Begin Source File

SOURCE=.\rep\rep_region.c
# End Source File
# Begin Source File

SOURCE=.\rep\rep_stat.c
# End Source File
# Begin Source File

SOURCE=.\rep\rep_util.c
# End Source File
# Begin Source File

SOURCE=.\hmac\sha1.c
# End Source File
# Begin Source File

SOURCE=.\clib\strcasecmp.c
# End Source File
# Begin Source File

SOURCE=.\txn\txn.c
# End Source File
# Begin Source File

SOURCE=.\txn\txn_auto.c
# End Source File
# Begin Source File

SOURCE=.\txn\txn_method.c
# End Source File
# Begin Source File

SOURCE=.\txn\txn_rec.c
# End Source File
# Begin Source File

SOURCE=.\txn\txn_recover.c
# End Source File
# Begin Source File

SOURCE=.\txn\txn_region.c
# End Source File
# Begin Source File

SOURCE=.\txn\txn_stat.c
# End Source File
# Begin Source File

SOURCE=.\txn\txn_util.c
# End Source File
# Begin Source File

SOURCE=.\common\util_log.c
# End Source File
# Begin Source File

SOURCE=.\common\util_sig.c
# End Source File
# Begin Source File

SOURCE=.\xa\xa.c
# End Source File
# Begin Source File

SOURCE=.\xa\xa_db.c
# End Source File
# Begin Source File

SOURCE=.\xa\xa_map.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# End Target
# End Project
