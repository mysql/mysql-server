
mkdir ..\bin
mkdir ..\bin\test
mkdir ..\lib
mkdir ..\obj
mkdir ..\obj\zlib

vacbld MySQL-Lib.icc -showprogress=10 -showwarning >> build-all.log
vacbld MySQL-Client.icc -showprogress=10 -showwarning >> build-all.log
vacbld MySQL-Sql.icc -showprogress=10 -showwarning >> build-all.log
vacbld MySQL-Util.icc -showprogress=10 -showwarning >> build-all.log
