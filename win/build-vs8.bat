@echo off

REM - First we need to copy all the cmakelists to the proper folders
cd win\cmakefiles
call deploy.bat
cd ..\..

del cmakecache.txt
copy win\vs8cache.txt cmakecache.txt
cmake -G "Visual Studio 8 2005"
copy cmakecache.txt win\vs8cache.txt
