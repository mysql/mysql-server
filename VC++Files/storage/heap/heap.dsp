# Microsoft Developer Studio Project File - Name="heap" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=heap - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "heap.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "heap.mak" CFG="heap - Win32 Debug"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "heap - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "heap - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "heap - Win32 TLS_DEBUG" (based on "Win32 (x86) Static Library")
!MESSAGE "heap - Win32 TLS" (based on "Win32 (x86) Static Library")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl6.exe
RSC=rc.exe

!IF  "$(CFG)" == "heap - Win32 Release"

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
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../../include" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=xilink6.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib_release\heap.lib"

!ELSEIF  "$(CFG)" == "heap - Win32 Debug"

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
# ADD CPP /nologo /G6 /MTd /W3 /Z7 /Od /Gf /I "../../include" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=xilink6.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\..\lib_debug\heap.lib"

!ELSEIF  "$(CFG)" == "heap - Win32 TLS_DEBUG"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "heap___Win32_TLS_DEBUG"
# PROP BASE Intermediate_Dir "heap___Win32_TLS_DEBUG"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "heap___Win32_TLS_DEBUG"
# PROP Intermediate_Dir "heap___Win32_TLS_DEBUG"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MTd /W3 /Z7 /Od /Gf /I "../../include" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /G6 /MTd /W3 /Z7 /Od /Gf /I "../../include" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /D "USE_TLS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\lib_debug\heap_tls.lib"
# ADD LIB32 /nologo /out:"..\..\lib_debug\heap_tls.lib"

!ELSEIF  "$(CFG)" == "heap - Win32 TLS"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "heap___Win32_TLS"
# PROP BASE Intermediate_Dir "heap___Win32_TLS"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "heap___Win32_TLS"
# PROP Intermediate_Dir "heap___Win32_TLS"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../../include" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../../include" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /D "USE_TLS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\lib_release\heap_tls.lib"
# ADD LIB32 /nologo /out:"..\..\lib_release\heap_tls.lib"

!ENDIF

# Begin Target

# Name "heap - Win32 Release"
# Name "heap - Win32 Debug"
# Name "heap - Win32 TLS_DEBUG"
# Name "heap - Win32 TLS"
# Begin Source File

SOURCE=.\_check.c
# End Source File
# Begin Source File

SOURCE=.\_rectest.c
# End Source File
# Begin Source File

SOURCE=.\heapdef.h
# End Source File
# Begin Source File

SOURCE=.\hp_block.c
# End Source File
# Begin Source File

SOURCE=.\hp_clear.c
# End Source File
# Begin Source File

SOURCE=.\hp_close.c
# End Source File
# Begin Source File

SOURCE=.\hp_create.c
# End Source File
# Begin Source File

SOURCE=.\hp_delete.c
# End Source File
# Begin Source File

SOURCE=.\hp_extra.c
# End Source File
# Begin Source File

SOURCE=.\hp_hash.c

!IF  "$(CFG)" == "heap - Win32 Release"

!ELSEIF  "$(CFG)" == "heap - Win32 Debug"

# SUBTRACT CPP /YX

!ELSEIF  "$(CFG)" == "heap - Win32 TLS_DEBUG"

# SUBTRACT BASE CPP /YX
# SUBTRACT CPP /YX

!ELSEIF  "$(CFG)" == "heap - Win32 TLS"

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\hp_info.c
# End Source File
# Begin Source File

SOURCE=.\hp_open.c
# End Source File
# Begin Source File

SOURCE=.\hp_panic.c
# End Source File
# Begin Source File

SOURCE=.\hp_rename.c
# End Source File
# Begin Source File

SOURCE=.\hp_rfirst.c
# End Source File
# Begin Source File

SOURCE=.\hp_rkey.c
# End Source File
# Begin Source File

SOURCE=.\hp_rlast.c
# End Source File
# Begin Source File

SOURCE=.\hp_rnext.c
# End Source File
# Begin Source File

SOURCE=.\hp_rprev.c
# End Source File
# Begin Source File

SOURCE=.\hp_rrnd.c
# End Source File
# Begin Source File

SOURCE=.\hp_rsame.c
# End Source File
# Begin Source File

SOURCE=.\hp_scan.c
# End Source File
# Begin Source File

SOURCE=.\hp_static.c
# End Source File
# Begin Source File

SOURCE=.\hp_update.c
# End Source File
# Begin Source File

SOURCE=.\hp_write.c
# End Source File
# End Target
# End Project
