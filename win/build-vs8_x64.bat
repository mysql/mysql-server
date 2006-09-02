@echo off

if exist cmakecache.txt del cmakecache.txt
copy win\vs8cache.txt cmakecache.txt
cmake -G "Visual Studio 8 2005 Win64"
copy cmakecache.txt win\vs8cache.txt
