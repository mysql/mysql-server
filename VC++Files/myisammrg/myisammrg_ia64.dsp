# Microsoft Developer Studio Project File - Name="myisammrg" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=myisammrg - WinIA64  TLS
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "myisammrg.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "myisammrg.mak" CFG="myisammrg - WinIA64  TLS"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "myisammrg - WinIA64  Release" (based on "Win32 (x86) Static Library")
!MESSAGE "myisammrg - WinIA64  Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "myisammrg - WinIA64  TLS_DEBUG" (based on "Win32 (x86) Static Library")
!MESSAGE "myisammrg - WinIA64  TLS" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "myisammrg - WinIA64  Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WinIA64" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /D "_IA64_" /D "WinIA64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_release\myisammrg.lib"

!ELSEIF  "$(CFG)" == "myisammrg - WinIA64  Debug"

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
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WinIA64" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Zi /Od /Gf /I "../include" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /D "_IA64_" /D "WinIA64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /Fo".\Debug/" /Fd".\Debug/" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_Debug\myisammrg.lib"

!ELSEIF  "$(CFG)" == "myisammrg - WinIA64  TLS_DEBUG"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "myisammrg___WinIA64 _TLS_DEBUG"
# PROP BASE Intermediate_Dir "myisammrg___WinIA64 _TLS_DEBUG"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "myisammrg___WinIA64 _TLS_DEBUG"
# PROP Intermediate_Dir "myisammrg___WinIA64 _TLS_DEBUG"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MTd /W3 /Z7 /Od /Gf /I "../include" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /Fo".\Debug/" /Fd".\Debug/" /FD /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MTd /W3 /Zi /O2 /I "../include" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /D "USE_TLS" /D "_IA64_" /D "WinIA64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /Fo".\Debug/" /Fd".\Debug/" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\lib_Debug\myisammrg_tls.lib"
# ADD LIB32 /nologo /out:"..\lib_Debug\myisammrg_tls.lib"

!ELSEIF  "$(CFG)" == "myisammrg - WinIA64  TLS"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "myisammrg___WinIA64 _TLS"
# PROP BASE Intermediate_Dir "myisammrg___WinIA64 _TLS"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "myisammrg___WinIA64 _TLS"
# PROP Intermediate_Dir "myisammrg___WinIA64 _TLS"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /D "USE_TLS" /D "_IA64_" /D "WinIA64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\lib_release\myisammrg_tls.lib"
# ADD LIB32 /nologo /out:"..\lib_release\myisammrg_tls.lib"

!ENDIF 

# Begin Target

# Name "myisammrg - WinIA64  Release"
# Name "myisammrg - WinIA64  Debug"
# Name "myisammrg - WinIA64  TLS_DEBUG"
# Name "myisammrg - WinIA64  TLS"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\myrg_close.c
# End Source File
# Begin Source File

SOURCE=.\myrg_create.c
# End Source File
# Begin Source File

SOURCE=.\myrg_delete.c
# End Source File
# Begin Source File

SOURCE=.\myrg_extra.c
# End Source File
# Begin Source File

SOURCE=.\myrg_info.c
# End Source File
# Begin Source File

SOURCE=.\myrg_locking.c
# End Source File
# Begin Source File

SOURCE=.\myrg_open.c
# End Source File
# Begin Source File

SOURCE=.\myrg_panic.c
# End Source File
# Begin Source File

SOURCE=.\myrg_queue.c
# End Source File
# Begin Source File

SOURCE=.\myrg_range.c
# End Source File
# Begin Source File

SOURCE=.\myrg_rfirst.c
# End Source File
# Begin Source File

SOURCE=.\myrg_rkey.c
# End Source File
# Begin Source File

SOURCE=.\myrg_rlast.c
# End Source File
# Begin Source File

SOURCE=.\myrg_rnext.c
# End Source File
# Begin Source File

SOURCE=.\myrg_rnext_same.c
# End Source File
# Begin Source File

SOURCE=.\myrg_rprev.c
# End Source File
# Begin Source File

SOURCE=.\myrg_rrnd.c
# End Source File
# Begin Source File

SOURCE=.\myrg_rsame.c
# End Source File
# Begin Source File

SOURCE=.\myrg_static.c
# End Source File
# Begin Source File

SOURCE=.\myrg_update.c
# End Source File
# Begin Source File

SOURCE=.\myrg_write.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\mymrgdef.h
# End Source File
# End Group
# End Target
# End Project
