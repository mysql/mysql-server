# Microsoft Developer Studio Project File - Name="MySqlManager" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=MYSQLMANAGER - WinIA64 DEBUG
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "MySqlManager.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "MySqlManager.mak" CFG="MYSQLMANAGER - WinIA64 DEBUG"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "MySqlManager - WinIA64 Release" (based on "Win32 (x86) Application")
!MESSAGE "MySqlManager - WinIA64 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "MySqlManager - WinIA64 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "release"
# PROP Intermediate_Dir "release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN64" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /I "../include" /D "NDEBUG" /D "DBUG_OFF" /D "_WINDOWS" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /WX /Fr /YX /Yc /Yu
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win64
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win64
# ADD BASE RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:IX86 /machine:IA64
# ADD LINK32  /nologo /subsystem:windows /machine:IX86 /out:"../client_release/MySqlManager.exe" /machine:IA64
# SUBTRACT LINK32 /nodefaultlib

!ELSEIF  "$(CFG)" == "MySqlManager - WinIA64 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "debug"
# PROP Intermediate_Dir "debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /Zi /Od /D "WIN64" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MTd /W3 /GR /Zi /Od /I "../include" /D "_DEBUG" /D "_WINDOWS" /D "_IA64_" /D "WIN64" /D "WIN32" /D "_AFX_NO_DAO_SUPPORT" /FD /G2 /EHsc /Wp64 /Zm600 /c
# SUBTRACT CPP /Fr /YX /Yc /Yu
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win64
# ADD MTL /nologo /D "_DEBUG" /o "NUL" /win64
# SUBTRACT MTL /mktyplib203
# ADD BASE RSC /l 0x409 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:IX86 /machine:IA64
# ADD LINK32 wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib uuid.lib /nologo /subsystem:windows /incremental:no /debug /machine:IX86 /out:"../client_debug/MySqlManager.exe" /libpath:"..\lib_debug\\" /machine:IA64

!ENDIF 

# Begin Target

# Name "MySqlManager - WinIA64 Release"
# Name "MySqlManager - WinIA64 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\ChildFrm.cpp
# End Source File
# Begin Source File

SOURCE="..\strings\ctype-extra.c"
# End Source File
# Begin Source File

SOURCE="..\strings\ctype-latin1.c"
# End Source File
# Begin Source File

SOURCE="..\strings\ctype-mb.c"
# End Source File
# Begin Source File

SOURCE=..\strings\is_prefix.c
# End Source File
# Begin Source File

SOURCE=.\MainFrm.cpp
# End Source File
# Begin Source File

SOURCE=..\mysys\my_sleep.c
# End Source File
# Begin Source File

SOURCE=..\strings\my_vsnprintf.c
# End Source File
# Begin Source File

SOURCE=.\MySqlManager.cpp
# End Source File
# Begin Source File

SOURCE=.\MySqlManager.rc
# End Source File
# Begin Source File

SOURCE=.\MySqlManagerDoc.cpp
# End Source File
# Begin Source File

SOURCE=.\MySqlManagerView.cpp
# End Source File
# Begin Source File

SOURCE=.\RegisterServer.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\ToolSql.cpp
# End Source File
# Begin Source File

SOURCE=.\ToolSqlQuery.cpp
# End Source File
# Begin Source File

SOURCE=.\ToolSqlResults.cpp
# End Source File
# Begin Source File

SOURCE=.\ToolSqlStatus.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\ChildFrm.h
# End Source File
# Begin Source File

SOURCE=.\MainFrm.h
# End Source File
# Begin Source File

SOURCE=.\MySqlManager.h
# End Source File
# Begin Source File

SOURCE=.\MySqlManagerDoc.h
# End Source File
# Begin Source File

SOURCE=.\MySqlManagerView.h
# End Source File
# Begin Source File

SOURCE=.\RegisterServer.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\ToolSqlQuery.h
# End Source File
# Begin Source File

SOURCE=.\ToolSqlResults.h
# End Source File
# Begin Source File

SOURCE=.\ToolSqlStatus.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;cnt;rtf;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\bitmap1.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bitmap3.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00001.bmp
# End Source File
# Begin Source File

SOURCE=.\res\bmp00002.bmp
# End Source File
# Begin Source File

SOURCE=.\res\database.bmp
# End Source File
# Begin Source File

SOURCE=.\res\fontd.bmp
# End Source File
# Begin Source File

SOURCE=.\res\fontu.bmp
# End Source File
# Begin Source File

SOURCE=.\res\MySqlManager.ico
# End Source File
# Begin Source File

SOURCE=.\res\MySqlManager.rc2
# End Source File
# Begin Source File

SOURCE=.\res\MySqlManagerDoc.ico
# End Source File
# Begin Source File

SOURCE=.\res\query_ex.bmp
# End Source File
# Begin Source File

SOURCE=.\res\Toolbar.bmp
# End Source File
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
