REM Copyright (c) 2006, 2012, Oracle and/or its affiliates. All rights reserved.
REM 
REM This program is free software; you can redistribute it and/or modify
REM it under the terms of the GNU General Public License, version 2.0,
REM as published by the Free Software Foundation.
REM
REM This program is also distributed with certain software (including
REM but not limited to OpenSSL) that is licensed under separate terms,
REM as designated in a particular file or component or in included license
REM documentation.  The authors of MySQL hereby grant you an additional
REM permission to link the program and your derivative works with the
REM separately licensed software that they have included with MySQL.
REM
REM This program is distributed in the hope that it will be useful,
REM but WITHOUT ANY WARRANTY; without even the implied warranty of
REM MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
REM GNU General Public License, version 2.0, for more details.
REM
REM You should have received a copy of the GNU General Public License
REM along with this program; if not, write to the Free Software
REM Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

REM quick and dirty build file for testing different MSDEVs
setlocal 

set myFLAGS= /I../include /I../taocrypt/include /I../taocrypt/mySTL /c /W3 /G6 /O2 /MT /D"WIN32" /D"NO_MAIN_DRIVER"

cl %myFLAGS% testsuite.cpp
cl %myFLAGS% ../examples/client/client.cpp
cl %myFLAGS% ../examples/echoclient/echoclient.cpp
cl %myFLAGS% ../examples/server/server.cpp
cl %myFLAGS% ../examples/echoserver/echoserver.cpp
cl %myFLAGS% ../taocrypt/test/test.cpp

link.exe  /out:testsuite.exe ../src/yassl.lib ../taocrypt/src/taocrypt.lib testsuite.obj client.obj server.obj echoclient.obj echoserver.obj test.obj advapi32.lib Ws2_32.lib

