@echo off
win32\cmakefiles\deploy
del cmakecache.txt
copy win32\vs71cache.txt cmakecache.txt
cmake -G "Visual Studio 7 .NET 2003"
copy cmakecache.txt win32\vs71cache.txt

