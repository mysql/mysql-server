# Microsoft Developer Studio Generated NMAKE File, Based on MySqlManager.dsp
!IF "$(CFG)" == ""
CFG=MySqlManager - Win32 Debug
!MESSAGE No configuration specified. Defaulting to MySqlManager - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "MySqlManager - Win32 Release" && "$(CFG)" != "MySqlManager - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "MySqlManager.mak" CFG="MySqlManager - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "MySqlManager - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "MySqlManager - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "MySqlManager - Win32 Release"

OUTDIR=.\release
INTDIR=.\release
# Begin Custom Macros
OutDir=.\release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "..\client_release\MySqlManager.exe" "$(OUTDIR)\MySqlManager.pch"

!ELSE 

ALL : "mysqlclient - Win32 Release" "..\client_release\MySqlManager.exe" "$(OUTDIR)\MySqlManager.pch"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"mysqlclient - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\ChildFrm.obj"
	-@erase "$(INTDIR)\MainFrm.obj"
	-@erase "$(INTDIR)\MySqlManager.obj"
	-@erase "$(INTDIR)\MySqlManager.pch"
	-@erase "$(INTDIR)\MySqlManager.res"
	-@erase "$(INTDIR)\MySqlManagerDoc.obj"
	-@erase "$(INTDIR)\MySqlManagerView.obj"
	-@erase "$(INTDIR)\RegisterServer.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\ToolSql.obj"
	-@erase "$(INTDIR)\ToolSqlQuery.obj"
	-@erase "$(INTDIR)\ToolSqlResults.obj"
	-@erase "$(INTDIR)\ToolSqlStatus.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "..\client_release\MySqlManager.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /G6 /MT /W3 /GX /O1 /I "../include" /D "NDEBUG" /D "DBUG_OFF" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /o "NUL" /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\MySqlManager.res" /d "NDEBUG" /d "_AFXDLL" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\MySqlManager.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=/nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\MySqlManager.pdb" /machine:I386 /out:"../client_release/MySqlManager.exe" 
LINK32_OBJS= \
	"$(INTDIR)\ChildFrm.obj" \
	"$(INTDIR)\MainFrm.obj" \
	"$(INTDIR)\MySqlManager.obj" \
	"$(INTDIR)\MySqlManagerDoc.obj" \
	"$(INTDIR)\MySqlManagerView.obj" \
	"$(INTDIR)\RegisterServer.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\ToolSql.obj" \
	"$(INTDIR)\ToolSqlQuery.obj" \
	"$(INTDIR)\ToolSqlResults.obj" \
	"$(INTDIR)\ToolSqlStatus.obj" \
	"$(INTDIR)\MySqlManager.res" \
	"..\lib_release\mysqlclient.lib"

"..\client_release\MySqlManager.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "MySqlManager - Win32 Debug"

OUTDIR=.\debug
INTDIR=.\debug
# Begin Custom Macros
OutDir=.\debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "..\client_debug\MySqlManager.exe" "$(OUTDIR)\MySqlManager.pch"

!ELSE 

ALL : "mysqlclient - Win32 Debug" "..\client_debug\MySqlManager.exe" "$(OUTDIR)\MySqlManager.pch"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"mysqlclient - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\ChildFrm.obj"
	-@erase "$(INTDIR)\MainFrm.obj"
	-@erase "$(INTDIR)\MySqlManager.obj"
	-@erase "$(INTDIR)\MySqlManager.pch"
	-@erase "$(INTDIR)\MySqlManager.res"
	-@erase "$(INTDIR)\MySqlManagerDoc.obj"
	-@erase "$(INTDIR)\MySqlManagerView.obj"
	-@erase "$(INTDIR)\RegisterServer.obj"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\ToolSql.obj"
	-@erase "$(INTDIR)\ToolSqlQuery.obj"
	-@erase "$(INTDIR)\ToolSqlResults.obj"
	-@erase "$(INTDIR)\ToolSqlStatus.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\MySqlManager.pdb"
	-@erase "..\client_debug\MySqlManager.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /G6 /MTd /W3 /Gm /GX /ZI /Od /I "../include" /D "_DEBUG" /D "_WINDOWS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
MTL_PROJ=/nologo /D "_DEBUG" /o "NUL" /win32 
RSC_PROJ=/l 0x409 /fo"$(INTDIR)\MySqlManager.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\MySqlManager.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib uuid.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\MySqlManager.pdb" /debug /machine:I386 /out:"../client_debug/MySqlManager.exe" /pdbtype:sept /libpath:"..\lib_debug\\" 
LINK32_OBJS= \
	"$(INTDIR)\ChildFrm.obj" \
	"$(INTDIR)\MainFrm.obj" \
	"$(INTDIR)\MySqlManager.obj" \
	"$(INTDIR)\MySqlManagerDoc.obj" \
	"$(INTDIR)\MySqlManagerView.obj" \
	"$(INTDIR)\RegisterServer.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\ToolSql.obj" \
	"$(INTDIR)\ToolSqlQuery.obj" \
	"$(INTDIR)\ToolSqlResults.obj" \
	"$(INTDIR)\ToolSqlStatus.obj" \
	"$(INTDIR)\MySqlManager.res" \
	"..\lib_debug\mysqlclient.lib"

"..\client_debug\MySqlManager.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("MySqlManager.dep")
!INCLUDE "MySqlManager.dep"
!ELSE 
!MESSAGE Warning: cannot find "MySqlManager.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "MySqlManager - Win32 Release" || "$(CFG)" == "MySqlManager - Win32 Debug"
SOURCE=.\ChildFrm.cpp

"$(INTDIR)\ChildFrm.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\MainFrm.cpp

"$(INTDIR)\MainFrm.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\MySqlManager.cpp

"$(INTDIR)\MySqlManager.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\MySqlManager.rc

"$(INTDIR)\MySqlManager.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)


SOURCE=.\MySqlManagerDoc.cpp

"$(INTDIR)\MySqlManagerDoc.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\MySqlManagerView.cpp

"$(INTDIR)\MySqlManagerView.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\RegisterServer.cpp

"$(INTDIR)\RegisterServer.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\StdAfx.cpp

!IF  "$(CFG)" == "MySqlManager - Win32 Release"

CPP_SWITCHES=/nologo /G6 /MT /W3 /GX /O1 /I "../include" /D "NDEBUG" /D "DBUG_OFF" /D "_WINDOWS" /Fp"$(INTDIR)\MySqlManager.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\MySqlManager.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "MySqlManager - Win32 Debug"

CPP_SWITCHES=/nologo /G6 /MTd /W3 /Gm /GX /ZI /Od /I "../include" /D "_DEBUG" /D "_WINDOWS" /Fp"$(INTDIR)\MySqlManager.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\MySqlManager.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=.\ToolSql.cpp

"$(INTDIR)\ToolSql.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ToolSqlQuery.cpp

"$(INTDIR)\ToolSqlQuery.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ToolSqlResults.cpp

"$(INTDIR)\ToolSqlResults.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\ToolSqlStatus.cpp

"$(INTDIR)\ToolSqlStatus.obj" : $(SOURCE) "$(INTDIR)"


!IF  "$(CFG)" == "MySqlManager - Win32 Release"

"mysqlclient - Win32 Release" : 
   cd "\MYSQL-3.23\client"
   $(MAKE) /$(MAKEFLAGS) /F ".\mysqlclient.mak" CFG="mysqlclient - Win32 Release" 
   cd "..\mysqlmanager"

"mysqlclient - Win32 ReleaseCLEAN" : 
   cd "\MYSQL-3.23\client"
   $(MAKE) /$(MAKEFLAGS) /F ".\mysqlclient.mak" CFG="mysqlclient - Win32 Release" RECURSE=1 CLEAN 
   cd "..\mysqlmanager"

!ELSEIF  "$(CFG)" == "MySqlManager - Win32 Debug"

"mysqlclient - Win32 Debug" : 
   cd "\MYSQL-3.23\client"
   $(MAKE) /$(MAKEFLAGS) /F ".\mysqlclient.mak" CFG="mysqlclient - Win32 Debug" 
   cd "..\mysqlmanager"

"mysqlclient - Win32 DebugCLEAN" : 
   cd "\MYSQL-3.23\client"
   $(MAKE) /$(MAKEFLAGS) /F ".\mysqlclient.mak" CFG="mysqlclient - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\mysqlmanager"

!ENDIF 


!ENDIF 

