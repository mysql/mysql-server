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

set myFLAGS= /I../include /I../mySTL /c /W3 /G6 /O2

cl %myFLAGS% test.cpp

link.exe  /out:test.exe ../src/taocrypt.lib test.obj advapi32.lib

