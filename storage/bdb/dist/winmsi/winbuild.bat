@echo off
::	$Id: winbuild.bat,v 1.4 2005/04/15 19:01:52 philipr Exp $
::	Helper script to build Berkeley DB libraries and executables
::	using MSDEV
::

cd build_win32

:: One of these calls should find the desired batch file

call :TryBat "c:\Program Files\Microsoft Visual Studio .NET 2003\Common7\Tools\vsvars32.bat" && goto BATFOUND1

call :TryBat "c:\Program Files\Microsoft Visual Studio .NET\Common7\Tools\vsvars32.bat" && goto BATFOUND2

call :TryBat "c:\Program Files\Microsoft Visual Studio.NET\Common7\Tools\vsvars32.bat" && goto BATFOUND3

goto BATNOTFOUND

:BATFOUND1
echo Using Visual Studio .NET 2003
goto BATFOUND

:BATFOUND2
echo Using Visual Studio .NET
echo *********** CHECK: Make sure the binaries are built with the same system libraries that are shipped.
goto BATFOUND

:BATFOUND3
echo Using Visual Studio.NET
echo *********** CHECK: Make sure the binaries are built with the same system libraries that are shipped.
goto BATFOUND

:BATFOUND
:CONVERSION
start /wait devenv /useenv Berkeley_DB.dsw

:: For some reason, the command doesn't wait, at least on XP.
:: So we ask for input to continue.


echo.
echo ============================================================
echo.
echo    Converting the Berkeley DB Workspace to a .NET Solution.
echo    This will run the IDE to interactively convert.
echo.
echo    When prompted during the conversion, say: Yes-to-All.
echo    When finished with the conversion, do a Save-All and Exit.
echo    Then hit ENTER to continue this script.
echo.
echo ============================================================
set result=y
set /P result="Continue? [y] "
if %result% == n goto NSTOP

if exist Berkeley_DB.sln goto ENDCONVERSION
echo ************* Berkeley_DB.sln was not created ***********
echo Trying the conversion again...
goto CONVERSION
:ENDCONVERSION

::intenv is used to set environment variables but this isn't used anymore
::devenv /useenv /build Release /project instenv ..\instenv\instenv.sln >> ..\winbld.out 2>&1
::if not %errorlevel% == 0 goto ERROR

echo Building Berkeley DB
devenv /useenv /build Debug /project build_all Berkeley_DB.sln >> ..\winbld.out 2>&1
if not %errorlevel% == 0 goto ERROR
devenv /useenv /build Release /project build_all Berkeley_DB.sln >> ..\winbld.out 2>&1
if not %errorlevel% == 0 goto ERROR
devenv /useenv /build "Debug Static" /project build_all Berkeley_DB.sln >> ..\winbld.out 2>&1
if not %errorlevel% == 0 goto ERROR
devenv /useenv /build "Release Static" /project build_all Berkeley_DB.sln >> ..\winbld.out 2>&1
if not %errorlevel% == 0 goto ERROR
devenv /useenv /build Debug /project ex_repquote Berkeley_DB.sln >> ..\winbld.out 2>&1
if not %errorlevel% == 0 goto ERROR
devenv /useenv /build Debug /project db_java Berkeley_DB.sln >> ..\winbld.out 2>&1
if not %errorlevel% == 0 goto ERROR
devenv /useenv /build Release /project db_java Berkeley_DB.sln >> ..\winbld.out 2>&1
if not %errorlevel% == 0 goto ERROR
devenv /useenv /build Debug /project db_tcl Berkeley_DB.sln >> ..\winbld.out 2>&1
if not %errorlevel% == 0 goto ERROR
devenv /useenv /build Release /project db_tcl Berkeley_DB.sln >> ..\winbld.out 2>&1
if not %errorlevel% == 0 goto ERROR


goto END


:ERROR
echo *********** ERROR: during win_build.bat *************
echo *********** ERROR: during win_build.bat *************  >> ..\winbld.err
exit 1
goto END

:NSTOP
echo *********** ERROR: win_build.bat stop requested *************
echo *********** ERROR: win_built.bat stop requested *************  >> ..\winbld.err
exit 2
goto END

:BATNOTFOUND
echo *********** ERROR: VC Config batch file not found *************
echo *********** ERROR: VC Config batch file not found *************  >> ..\winbld.err
exit 3
goto END

:: TryBat(BATPATH)
:: If the BATPATH exists, use it and return 0,
:: otherwise, return 1.

:TryBat
:: Filename = %1
if not exist %1 exit /b 1
call %1
exit /b 0
goto :EOF

:END
