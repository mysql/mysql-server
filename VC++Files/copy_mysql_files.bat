REM stop any conflicting service

net stop mysql

REM Copy binaries to c:\mysql
REM (We assume we are in build root when executing this script)

copy lib_debug\libmysql.* c:\mysql\lib\debug
copy lib_debug\zlib.* c:\mysql\lib\debug
copy lib_debug\mysqlclient.lib c:\mysql\lib\debug

copy lib_release\mysqlclient.lib c:\mysql\lib\opt
copy lib_release\libmysql.* c:\mysql\lib\opt
copy lib_release\zlib.* c:\mysql\lib\opt

IF "%1"=="classic" goto CLASSIC
IF "%1"=="pro" goto PRO

REM GPL binaries

copy client_release\*.exe c:\mysql\bin
copy client_debug\mysqld.exe c:\mysql\bin

goto REST

:CLASSIC
REM Classic binaries

copy client_release\*.exe c:\mysql\bin
copy client_classic\*.exe c:\mysql\bin
copy client_debug\mysqld.exe c:\mysql\bin\mysqld-debug.exe
copy lib_classic\*.* c:\mysql\lib\opt

goto REST

:PRO
REM Pro binaries

copy client_release\*.exe c:\mysql\bin
copy client_classic\*.exe c:\mysql\bin
copy client_pro\*.exe c:\mysql\bin
copy client_debug\mysqld.exe c:\mysql\bin\mysqld-debug.exe
copy lib_pro\*.* c:\mysql\lib\opt

:REST

REM
REM Copy include files
REM

copy include\mysql*.h c:\mysql\include
copy include\errmsg.h c:\mysql\include
copy include\my_sys.h c:\mysql\include
copy include\my_list.h c:\mysql\include
copy include\my_pthread.h c:\mysql\include
copy include\my_dbug.h c:\mysql\include
copy include\m_string.h c:\mysql\include
copy include\m_ctype.h c:\mysql\include
copy include\raid.h c:\mysql\include
copy include\conf*.h c:\mysql\include
copy include\my_global.h c:\mysql\include\my_global.h
copy libmysql\libmysql.def c:\mysql\include

REM Copy test files

copy libmysqltest\*.* c:\mysql\examples\libmysqltest
copy libmysqltest\release\myTest.exe c:\mysql\examples\libmysqltest

REM Copy share, docs etc

xcopy share\*.* c:\mysql\share /E /Y
xcopy scripts\*.* c:\mysql\scripts /E /Y
xcopy docs\*.* c:\mysql\docs /E /Y
xcopy sql-bench\*.* c:\mysql\bench /E /Y
copy docs\readme c:\mysql\

REM Copy privilege tables (Delete old ones as they may be from a newer version)

del c:\mysql\data\mysql\*.* /Q
xcopy data\mysql\*.* c:\mysql\data\mysql /E /Y
