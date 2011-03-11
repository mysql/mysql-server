set build_64_bit=
set build_msi=
set generator=
set scriptdir=%~dp0

::     Process all the arguments from the command line
::
:process_arguments
  if "%~1"=="" goto :do_work
  if "%~1"=="-h" goto :help
  if "%~1"=="-msi" set build_msi=1
  if "%~1"=="-G" set generator=-G "%~2"
  shift
  goto :process_arguments

:help
 echo "build_maria_release [-h] [-msi] [-G <Generator>]" 

:die
 echo error occured.
 popd
 exit /b 1

:do_work
:: We're doing out-of-source build to ensure nobody has broken it:)
 
  pushd %scriptdir%
  cd ..
  rd /s /q xxx
  mkdir xxx
  cd xxx
  
  cmake .. -DWITH_EMBEDDED_SERVER=1 %generator%
  if %ERRORLEVEL% NEQ 0  goto :die
  cmake --build . --config Debug
  if %ERRORLEVEL% NEQ 0  goto :die
  cmake --build . --config RelWithDebInfo --target package
  if %ERRORLEVEL% NEQ 0  goto :die
  
  
  if "%build_msi%"=="1" (
    cmake --build . --config RelWithDebInfo --target win\packaging\msi
    if %ERRORLEVEL% NEQ 0  goto :die
  )
  xcopy /y *.zip  ..
  xcopy /y *.msi ..
  popd