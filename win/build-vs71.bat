@echo off

if exist cmakecache.txt del cmakecache.txt
copy win\vs71cache.txt cmakecache.txt
cmake -G "Visual Studio 7 .NET 2003"
copy cmakecache.txt win\vs71cache.txt

