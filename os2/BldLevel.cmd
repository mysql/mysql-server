@echo off

REM I'm using resources for BLDLEVEL info, because VA4 linker has the bad
REM feature of using versionstring content for padding files.

REM To set fixpak level: -P"fixpak level"
SET MYSQL_VERSION=3.23.50
SET MYSQL_BUILD=B1

BldLevelInf -V%MYSQL_VERSION% -N"MySQL AB, Yuri Dario" -D"MySQL %MYSQL_VERSION% for OS/2 - Build %MYSQL_BUILD%" -Len BldLevel.rc
