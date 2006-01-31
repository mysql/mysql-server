@echo off

REM - First we need to copy all the cmakelists to the proper folders
cd win\cmakefiles
call deploy.bat
cd ..\..

del cmakecache.txt
copy win\vs71cache.txt cmakecache.txt
cmake -G "Visual Studio 7 .NET 2003"
copy cmakecache.txt win\vs71cache.txt

