# Microsoft Developer Studio Project File - Name="isam" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=isam - WinIA64 TLS
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "isam.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "isam.mak" CFG="isam - WinIA64 TLS"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "isam - WinIA64 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "isam - WinIA64 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "isam - WinIA64 TLS_DEBUG" (based on "Win32 (x86) Static Library")
!MESSAGE "isam - WinIA64 TLS" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "isam - WinIA64 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN64" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_release\isam.lib"

!ELSEIF  "$(CFG)" == "isam - WinIA64 Debug"

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
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN64" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Zi /Od /GF /I "../include" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_Debug\isam.lib"

!ELSEIF  "$(CFG)" == "isam - WinIA64 TLS_DEBUG"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "isam___WinIA64_TLS_DEBUG"
# PROP BASE Intermediate_Dir "isam___WinIA64_TLS_DEBUG"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "isam___WinIA64_TLS_DEBUG"
# PROP Intermediate_Dir "isam___WinIA64_TLS_DEBUG"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MTd /W3 /Z7 /Od /GF /I "../include" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MTd /W3 /Zi /O2 /I "../include" /D "_DEBUG" /D "SAFEMALLOC" /D "SAFE_MUTEX" /D "_WINDOWS" /D "USE_TLS" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\lib_Debug\isam_tls.lib"
# ADD LIB32 /nologo /out:"..\lib_Debug\isam_tls.lib"

!ELSEIF  "$(CFG)" == "isam - WinIA64 TLS"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "isam___WinIA64_TLS"
# PROP BASE Intermediate_Dir "isam___WinIA64_TLS"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "isam___WinIA64_TLS"
# PROP Intermediate_Dir "isam___WinIA64_TLS"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /O2 /I "../include" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /D "DBUG_OFF" /D "_WINDOWS" /D "NDEBUG" /D "USE_TLS" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\lib_release\isam_tls.lib"
# ADD LIB32 /nologo /out:"..\lib_release\isam_tls.lib"

!ENDIF 

# Begin Target

# Name "isam - WinIA64 Release"
# Name "isam - WinIA64 Debug"
# Name "isam - WinIA64 TLS_DEBUG"
# Name "isam - WinIA64 TLS"
# Begin Source File

SOURCE=.\_cache.c
# End Source File
# Begin Source File

SOURCE=.\_dbug.c
# End Source File
# Begin Source File

SOURCE=.\_dynrec.c
# End Source File
# Begin Source File

SOURCE=.\_key.c
# End Source File
# Begin Source File

SOURCE=.\_locking.c
# End Source File
# Begin Source File

SOURCE=.\_packrec.c
# End Source File
# Begin Source File

SOURCE=.\_page.c
# End Source File
# Begin Source File

SOURCE=.\_search.c
# End Source File
# Begin Source File

SOURCE=.\_statrec.c
# End Source File
# Begin Source File

SOURCE=.\changed.c
# End Source File
# Begin Source File

SOURCE=.\close.c
# End Source File
# Begin Source File

SOURCE=.\create.c
# End Source File
# Begin Source File

SOURCE=.\delete.c
# End Source File
# Begin Source File

SOURCE=.\extra.c
# End Source File
# Begin Source File

SOURCE=.\info.c
# End Source File
# Begin Source File

SOURCE=.\log.c
# End Source File
# Begin Source File

SOURCE=.\open.c
# End Source File
# Begin Source File

SOURCE=.\panic.c
# End Source File
# Begin Source File

SOURCE=.\range.c
# End Source File
# Begin Source File

SOURCE=.\rfirst.c
# End Source File
# Begin Source File

SOURCE=.\rkey.c
# End Source File
# Begin Source File

SOURCE=.\rlast.c
# End Source File
# Begin Source File

SOURCE=.\rnext.c
# End Source File
# Begin Source File

SOURCE=.\rprev.c
# End Source File
# Begin Source File

SOURCE=.\rrnd.c
# End Source File
# Begin Source File

SOURCE=.\rsame.c
# End Source File
# Begin Source File

SOURCE=.\rsamepos.c
# End Source File
# Begin Source File

SOURCE=.\static.c
# End Source File
# Begin Source File

SOURCE=.\update.c
# End Source File
# Begin Source File

SOURCE=.\write.c
# End Source File
# End Target
# End Project
