@echo off
win32\cmakefiles\deploy
del cmakecache.txt
copy win32\vs8cache.txt cmakecache.txt
cmake -G "Visual Studio 8 2005"
copy cmakecache.txt win32\vs8cache.txt
