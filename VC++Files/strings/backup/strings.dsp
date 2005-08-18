# Microsoft Developer Studio Project File - Name="strings" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=strings - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE
!MESSAGE NMAKE /f "strings.mak".
!MESSAGE
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE
!MESSAGE NMAKE /f "strings.mak" CFG="strings - Win32 Debug"
!MESSAGE
!MESSAGE Possible choices for configuration are:
!MESSAGE
!MESSAGE "strings - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "strings - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "strings - Win32 Release"

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
# ADD CPP /nologo /G6 /MT /W3 /O2 /I "../include" /D "NDEBUG" /D "DBUG_OFF" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_release\strings.lib"

!ELSEIF  "$(CFG)" == "strings - Win32 Debug"

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
# ADD CPP /nologo /G6 /MTd /W3 /Z7 /Od /Gf /I "../include" /D "_DEBUG" /D "SAFEMALLOC" /D "_WINDOWS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\lib_debug\strings.lib"

!ENDIF

# Begin Target

# Name "strings - Win32 Release"
# Name "strings - Win32 Debug"
# Begin Source File

SOURCE=.\atof.c
# End Source File
# Begin Source File

SOURCE=.\bchange.c
# End Source File
# Begin Source File

SOURCE=.\bcmp.c
# End Source File
# Begin Source File

SOURCE=.\bfill.c
# End Source File
# Begin Source File

SOURCE=.\bmove512.c
# End Source File
# Begin Source File

SOURCE=".\ctype-big5.c"
# End Source File
# Begin Source File

SOURCE=".\ctype-czech.c"
# End Source File
# Begin Source File

SOURCE=".\ctype-euc_kr.c"
# End Source File
# Begin Source File

SOURCE=".\ctype-gb2312.c"
# End Source File
# Begin Source File

SOURCE=".\ctype-gbk.c"
# End Source File
# Begin Source File

SOURCE=".\ctype-sjis.c"
# End Source File
# Begin Source File

SOURCE=".\ctype-tis620.c"
# End Source File
# Begin Source File

SOURCE=".\ctype-ujis.c"
# End Source File
# Begin Source File

SOURCE=.\ctype.c
# End Source File
# Begin Source File

SOURCE=.\int2str.c
# End Source File
# Begin Source File

SOURCE=.\llstr.c
# End Source File
# Begin Source File

SOURCE=.\longlong2str.c
# End Source File
# Begin Source File

SOURCE=.\r_strinstr.c
# End Source File
# Begin Source File

SOURCE=.\str2int.c
# End Source File
# Begin Source File

SOURCE=.\Strings.asm

!IF  "$(CFG)" == "strings - Win32 Release"

# Begin Custom Build
OutDir=.\release
InputPath=.\Strings.asm
InputName=Strings

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	C:\masm\masm -Mx  -t -DDOS386 -DM_I386   $(InputPath),$(OutDir)\$(InputName).obj,,,

# End Custom Build

!ELSEIF  "$(CFG)" == "strings - Win32 Debug"

# Begin Custom Build
OutDir=.\debug
InputPath=.\Strings.asm
InputName=Strings

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	C:\masm\masm -Mx  -t -DDOS386 -DM_I386   $(InputPath),$(Outdir)\$(InputName).obj,,,

# End Custom Build

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\strtol.c
# End Source File
# Begin Source File

SOURCE=.\strtoll.c
# End Source File
# Begin Source File

SOURCE=.\strtoul.c
# End Source File
# Begin Source File

SOURCE=.\strtoull.c
# End Source File
# Begin Source File

SOURCE=.\Strxmov.asm

!IF  "$(CFG)" == "strings - Win32 Release"

# Begin Custom Build
OutDir=.\release
InputPath=.\Strxmov.asm
InputName=Strxmov

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	C:\masm\masm -Mx  -t -DDOS386 -DM_I386   $(InputPath),$(OutDir)\$(InputName).obj,,,

# End Custom Build

!ELSEIF  "$(CFG)" == "strings - Win32 Debug"

# Begin Custom Build
OutDir=.\debug
InputPath=.\Strxmov.asm
InputName=Strxmov

"$(OutDir)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	C:\masm\masm -Mx  -t -DDOS386 -DM_I386   $(InputPath),$(Outdir)\$(InputName).obj,,,

# End Custom Build

!ENDIF

# End Source File
# Begin Source File

SOURCE=.\strxnmov.c 
# End Source File 
# Begin Source File

SOURCE=.\str_alloc.c 
# End Source File

# End Target
# End Project
