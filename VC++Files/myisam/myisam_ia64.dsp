# Microsoft Developer Studio Project File - Name="myisam" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=myisam - WinIA64 TLS
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "myisam.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "myisam.mak" CFG="myisam - WinIA64 TLS"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "myisam - WinIA64 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "myisam - WinIA64 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "myisam - WinIA64 TLS_DEBUG" (based on "Win32 (x86) Static Library")
!MESSAGE "myisam - WinIA64 TLS" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "myisam - WinIA64 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN64" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_release\myisam.lib"

!ELSEIF  "$(CFG)" == "myisam - WinIA64 Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN64" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Zi /Od /GF /I "../include" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /Fo".\Debug/" /Fd".\Debug/" /FD /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32  /nologo
# ADD LIB32 /nologo /out:"..\lib_Debug\myisam.lib"

!ELSEIF  "$(CFG)" == "myisam - WinIA64 TLS_DEBUG"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "myisam___Win64_TLS_DEBUG"
# PROP BASE Intermediate_Dir "myisam___Win64_TLS_DEBUG"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "myisam___Win64_TLS_DEBUG"
# PROP Intermediate_Dir "myisam___Win64_TLS_DEBUG"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MTd /W3 /Z7 /Od /GF /I "../include" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /Fo".\Debug/" /Fd".\Debug/" /FD /c
# ADD CPP /nologo /MTd /W3 /Zi /O2 /I "../include" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /D "USE_TLS" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /Fo".\Debug/" /Fd".\Debug/" /FD /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32  /nologo /out:"..\lib_Debug\myisam_tls.lib"
# ADD LIB32 /nologo /out:"..\lib_Debug\myisam_tls.lib"

!ELSEIF  "$(CFG)" == "myisam - WinIA64 TLS"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "myisam___Win64_TLS"
# PROP BASE Intermediate_Dir "myisam___Win64_TLS"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "myisam___Win64_TLS"
# PROP Intermediate_Dir "myisam___Win64_TLS"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /D "USE_TLS" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32  /nologo /out:"..\lib_release\myisam_tls.lib"
# ADD LIB32 /nologo /out:"..\lib_release\myisam_tls.lib"

!ENDIF 

# Begin Target

# Name "myisam - WinIA64 Release"
# Name "myisam - WinIA64 Debug"
# Name "myisam - WinIA64 TLS_DEBUG"
# Name "myisam - WinIA64 TLS"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\ft_boolean_search.c
# End Source File
# Begin Source File

SOURCE=.\ft_nlq_search.c
# End Source File
# Begin Source File

SOURCE=.\ft_parser.c
# End Source File
# Begin Source File

SOURCE=.\ft_static.c
# End Source File
# Begin Source File

SOURCE=.\ft_stem.c
# End Source File
# Begin Source File

SOURCE=.\ft_stopwords.c
# End Source File
# Begin Source File

SOURCE=.\ft_update.c
# End Source File
# Begin Source File

SOURCE=.\mi_cache.c
# End Source File
# Begin Source File

SOURCE=.\mi_changed.c
# End Source File
# Begin Source File

SOURCE=.\mi_check.c
# End Source File
# Begin Source File

SOURCE=.\mi_checksum.c
# End Source File
# Begin Source File

SOURCE=.\mi_close.c
# End Source File
# Begin Source File

SOURCE=.\mi_create.c
# End Source File
# Begin Source File

SOURCE=.\mi_dbug.c
# End Source File
# Begin Source File

SOURCE=.\mi_delete.c
# End Source File
# Begin Source File

SOURCE=.\mi_delete_all.c
# End Source File
# Begin Source File

SOURCE=.\mi_delete_table.c
# End Source File
# Begin Source File

SOURCE=.\mi_dynrec.c
# End Source File
# Begin Source File

SOURCE=.\mi_extra.c
# End Source File
# Begin Source File

SOURCE=.\mi_info.c
# End Source File
# Begin Source File

SOURCE=.\mi_key.c
# End Source File
# Begin Source File

SOURCE=.\mi_keycache.c
# End Source File
# Begin Source File

SOURCE=.\mi_locking.c
# End Source File
# Begin Source File

SOURCE=.\mi_log.c
# End Source File
# Begin Source File

SOURCE=.\mi_open.c
# End Source File
# Begin Source File

SOURCE=.\mi_packrec.c
# End Source File
# Begin Source File

SOURCE=.\mi_page.c
# End Source File
# Begin Source File

SOURCE=.\mi_panic.c
# End Source File
# Begin Source File

SOURCE=.\mi_preload.c
# End Source File
# Begin Source File

SOURCE=.\mi_range.c
# End Source File
# Begin Source File

SOURCE=.\mi_rename.c
# End Source File
# Begin Source File

SOURCE=.\mi_rfirst.c
# End Source File
# Begin Source File

SOURCE=.\mi_rkey.c
# End Source File
# Begin Source File

SOURCE=.\mi_rlast.c
# End Source File
# Begin Source File

SOURCE=.\mi_rnext.c
# End Source File
# Begin Source File

SOURCE=.\mi_rnext_same.c
# End Source File
# Begin Source File

SOURCE=.\mi_rprev.c
# End Source File
# Begin Source File

SOURCE=.\mi_rrnd.c
# End Source File
# Begin Source File

SOURCE=.\mi_rsame.c
# End Source File
# Begin Source File

SOURCE=.\mi_rsamepos.c
# End Source File
# Begin Source File

SOURCE=.\mi_scan.c
# End Source File
# Begin Source File

SOURCE=.\mi_search.c
# End Source File
# Begin Source File

SOURCE=.\mi_static.c
# End Source File
# Begin Source File

SOURCE=.\mi_statrec.c
# End Source File
# Begin Source File

SOURCE=.\mi_unique.c
# End Source File
# Begin Source File

SOURCE=.\mi_update.c
# End Source File
# Begin Source File

SOURCE=.\mi_write.c
# End Source File
# Begin Source File

SOURCE=.\rt_index.c
# End Source File
# Begin Source File

SOURCE=.\rt_key.c
# End Source File
# Begin Source File

SOURCE=.\rt_mbr.c
# End Source File
# Begin Source File

SOURCE=.\rt_split.c
# End Source File
# Begin Source File

SOURCE=.\sort.c
# End Source File
# Begin Source File

SOURCE=.\sp_key.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\ft_eval.h
# End Source File
# Begin Source File

SOURCE=.\myisamdef.h
# End Source File
# Begin Source File

SOURCE=.\rt_index.h
# End Source File
# End Group
# End Target
# End Project
