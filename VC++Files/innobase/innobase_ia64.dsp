# Microsoft Developer Studio Project File - Name="innobase" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=innobase - WinIA64 Max nt
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "innobase.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "innobase.mak" CFG="innobase - WinIA64 Max nt"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "innobase - WinIA64 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "innobase - WinIA64 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "innobase - WinIA64 nt" (based on "Win32 (x86) Static Library")
!MESSAGE "innobase - WinIA64 Max nt" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "innobase - WinIA64 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "innobase___WinIA64_Debug"
# PROP BASE Intermediate_Dir "innobase___WinIA64_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /GX /O2 /I "../innobase/include" /D "NDEBUG" /D "_LIB" /D "_WIN64" /D "__NT__" /D "WIN64" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Zi /Od /I "../innobase/include" /I "../include" /D "NDEBUG" /D "_LIB" /D "_WIN64" /D "WIN64" /D "_MBCS" /D "MYSQL_SERVER" /D "_IA64_" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /Fr /YX
# ADD BASE RSC /l 0x416 /d "NDEBUG"
# ADD RSC /l 0x416 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\lib_release\innobase-nt.lib"
# ADD LIB32 /nologo /out:"..\lib_debug\innodb.lib"

!ELSEIF  "$(CFG)" == "innobase - WinIA64 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "innobase___WinIA64_Release0"
# PROP BASE Intermediate_Dir "innobase___WinIA64_Release0"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "release"
# PROP Intermediate_Dir "release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /GX /O2 /I "../innobase/include" /I "../include" /D "NDEBUG" /D "_LIB" /D "_WIN64" /D "WIN64" /D "_MBCS" /D "MYSQL_SERVER" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../innobase/include" /I "../include" /D "_LIB" /D "_WIN64" /D "WIN64" /D "_MBCS" /D "MYSQL_SERVER" /D "NDEBUG" /D "_IA64_" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /WX /Fr /YX
# ADD BASE RSC /l 0x416 /d "NDEBUG"
# ADD RSC /l 0x416 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\lib_release\innodb.lib"
# ADD LIB32 /nologo /out:"..\lib_release\innodb.lib"

!ELSEIF  "$(CFG)" == "innobase - WinIA64 nt"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "innobase___WinIA64_nt"
# PROP BASE Intermediate_Dir "innobase___WinIA64_nt"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "nt"
# PROP Intermediate_Dir "nt"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /GX /O2 /I "../innobase/include" /I "../include" /D "NDEBUG" /D "_LIB" /D "_WIN64" /D "WIN64" /D "_MBCS" /D "MYSQL_SERVER" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../innobase/include" /I "../include" /D "_LIB" /D "_WIN64" /D "WIN64" /D "NDEBUG" /D "MYSQL_SERVER" /D "_MBCS" /D "_IA64_" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /DMYSQL_SERVER_SUFFIX=-nt /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x416 /d "NDEBUG"
# ADD RSC /l 0x416 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\lib_release\innodb.lib"
# ADD LIB32 /nologo /out:"..\lib_release\innodb.lib"

!ELSEIF  "$(CFG)" == "innobase - WinIA64 Max nt"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "innobase___WinIA64_Max_nt"
# PROP BASE Intermediate_Dir "innobase___WinIA64_Max_nt"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "max_nt"
# PROP Intermediate_Dir "max_nt"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /G6 /MT /W3 /GX /O2 /I "../innobase/include" /I "../include" /D "NDEBUG" /D "_LIB" /D "_Win64" /D "WIN64" /D "_MBCS" /D "MYSQL_SERVER" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../innobase/include" /I "../include" /D "_LIB" /D "_WIN64" /D "WIN64" /D "NDEBUG" /D "MYSQL_SERVER" /D "_MBCS" /D "_IA64_" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /DMYSQL_SERVER_SUFFIX=-nt-max /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x416 /d "NDEBUG"
# ADD RSC /l 0x416 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\lib_release\innodb.lib"
# ADD LIB32 /nologo /out:"..\lib_release\innodb.lib"

!ENDIF 

# Begin Target

# Name "innobase - WinIA64 Debug"
# Name "innobase - WinIA64 Release"
# Name "innobase - WinIA64 nt"
# Name "innobase - WinIA64 Max nt"
# Begin Source File

SOURCE=.\btr\btr0btr.c
# End Source File
# Begin Source File

SOURCE=.\btr\btr0cur.c
# End Source File
# Begin Source File

SOURCE=.\btr\btr0pcur.c
# End Source File
# Begin Source File

SOURCE=.\btr\btr0sea.c
# End Source File
# Begin Source File

SOURCE=.\buf\buf0buf.c
# End Source File
# Begin Source File

SOURCE=.\buf\buf0flu.c
# End Source File
# Begin Source File

SOURCE=.\buf\buf0lru.c
# End Source File
# Begin Source File

SOURCE=.\buf\buf0rea.c
# End Source File
# Begin Source File

SOURCE=.\data\data0data.c
# End Source File
# Begin Source File

SOURCE=.\data\data0type.c
# End Source File
# Begin Source File

SOURCE=.\dict\dict0boot.c
# End Source File
# Begin Source File

SOURCE=.\dict\dict0crea.c
# End Source File
# Begin Source File

SOURCE=.\dict\dict0dict.c
# End Source File
# Begin Source File

SOURCE=.\dict\dict0load.c
# End Source File
# Begin Source File

SOURCE=.\dict\dict0mem.c
# End Source File
# Begin Source File

SOURCE=.\dyn\dyn0dyn.c
# End Source File
# Begin Source File

SOURCE=.\eval\eval0eval.c
# End Source File
# Begin Source File

SOURCE=.\eval\eval0proc.c
# End Source File
# Begin Source File

SOURCE=.\fil\fil0fil.c
# End Source File
# Begin Source File

SOURCE=.\fsp\fsp0fsp.c
# End Source File
# Begin Source File

SOURCE=.\fut\fut0fut.c
# End Source File
# Begin Source File

SOURCE=.\fut\fut0lst.c
# End Source File
# Begin Source File

SOURCE=.\ha\ha0ha.c
# End Source File
# Begin Source File

SOURCE=.\ha\hash0hash.c
# End Source File
# Begin Source File

SOURCE=.\ibuf\ibuf0ibuf.c
# End Source File
# Begin Source File

SOURCE=.\pars\lexyy.c
# End Source File
# Begin Source File

SOURCE=.\lock\lock0lock.c
# End Source File
# Begin Source File

SOURCE=.\log\log0log.c
# End Source File
# Begin Source File

SOURCE=.\log\log0recv.c
# End Source File
# Begin Source File

SOURCE=.\mach\mach0data.c
# End Source File
# Begin Source File

SOURCE=.\mem\mem0mem.c
# End Source File
# Begin Source File

SOURCE=.\mem\mem0pool.c
# End Source File
# Begin Source File

SOURCE=.\mtr\mtr0log.c
# End Source File
# Begin Source File

SOURCE=.\mtr\mtr0mtr.c
# End Source File
# Begin Source File

SOURCE=.\os\os0file.c
# End Source File
# Begin Source File

SOURCE=.\os\os0proc.c
# End Source File
# Begin Source File

SOURCE=.\os\os0sync.c
# End Source File
# Begin Source File

SOURCE=.\os\os0thread.c
# End Source File
# Begin Source File

SOURCE=.\page\page0cur.c
# End Source File
# Begin Source File

SOURCE=.\page\page0page.c
# End Source File
# Begin Source File

SOURCE=.\pars\pars0grm.c
# End Source File
# Begin Source File

SOURCE=.\pars\pars0opt.c
# End Source File
# Begin Source File

SOURCE=.\pars\pars0pars.c
# End Source File
# Begin Source File

SOURCE=.\pars\pars0sym.c
# End Source File
# Begin Source File

SOURCE=.\que\que0que.c
# End Source File
# Begin Source File

SOURCE=.\read\read0read.c
# End Source File
# Begin Source File

SOURCE=.\rem\rem0cmp.c
# End Source File
# Begin Source File

SOURCE=.\rem\rem0rec.c
# End Source File
# Begin Source File

SOURCE=.\row\row0ins.c
# End Source File
# Begin Source File

SOURCE=.\row\row0mysql.c
# End Source File
# Begin Source File

SOURCE=.\row\row0purge.c
# End Source File
# Begin Source File

SOURCE=.\row\row0row.c
# End Source File
# Begin Source File

SOURCE=.\row\row0sel.c
# End Source File
# Begin Source File

SOURCE=.\row\row0uins.c
# End Source File
# Begin Source File

SOURCE=.\row\row0umod.c
# End Source File
# Begin Source File

SOURCE=.\row\row0undo.c
# End Source File
# Begin Source File

SOURCE=.\row\row0upd.c
# End Source File
# Begin Source File

SOURCE=.\row\row0vers.c
# End Source File
# Begin Source File

SOURCE=.\srv\srv0que.c
# End Source File
# Begin Source File

SOURCE=.\srv\srv0srv.c
# End Source File
# Begin Source File

SOURCE=.\srv\srv0start.c
# End Source File
# Begin Source File

SOURCE=.\sync\sync0arr.c
# End Source File
# Begin Source File

SOURCE=.\sync\sync0rw.c
# End Source File
# Begin Source File

SOURCE=.\sync\sync0sync.c
# End Source File
# Begin Source File

SOURCE=.\thr\thr0loc.c
# End Source File
# Begin Source File

SOURCE=.\trx\trx0purge.c
# End Source File
# Begin Source File

SOURCE=.\trx\trx0rec.c
# End Source File
# Begin Source File

SOURCE=.\trx\trx0roll.c
# End Source File
# Begin Source File

SOURCE=.\trx\trx0rseg.c
# End Source File
# Begin Source File

SOURCE=.\trx\trx0sys.c
# End Source File
# Begin Source File

SOURCE=.\trx\trx0trx.c
# End Source File
# Begin Source File

SOURCE=.\trx\trx0undo.c
# End Source File
# Begin Source File

SOURCE=.\usr\usr0sess.c
# End Source File
# Begin Source File

SOURCE=.\ut\ut0byte.c
# End Source File
# Begin Source File

SOURCE=.\ut\ut0dbg.c
# End Source File
# Begin Source File

SOURCE=.\ut\ut0mem.c
# End Source File
# Begin Source File

SOURCE=.\ut\ut0rnd.c
# End Source File
# Begin Source File

SOURCE=.\ut\ut0ut.c
# End Source File
# End Target
# End Project
