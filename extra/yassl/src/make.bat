REM Copyright (C) 2006, 2007 MySQL AB
REM 
REM This program is free software; you can redistribute it and/or modify
REM it under the terms of the GNU General Public License as published by
REM the Free Software Foundation; version 2 of the License.
REM 
REM This program is distributed in the hope that it will be useful,
REM but WITHOUT ANY WARRANTY; without even the implied warranty of
REM MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
REM GNU General Public License for more details.
REM 
REM You should have received a copy of the GNU General Public License
REM along with this program; if not, write to the Free Software
REM Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

REM quick and dirty build file for testing different MSDEVs
setlocal 

set myFLAGS= /I../include /I../taocrypt/mySTL /I../taocrypt/include /W3 /c /ZI

cl %myFLAGS% buffer.cpp
cl %myFLAGS% cert_wrapper.cpp
cl %myFLAGS% crypto_wrapper.cpp
cl %myFLAGS% handshake.cpp

cl %myFLAGS% lock.cpp
cl %myFLAGS% log.cpp
cl %myFLAGS% socket_wrapper.cpp
cl %myFLAGS% ssl.cpp

cl %myFLAGS% template_instnt.cpp
cl %myFLAGS% timer.cpp
cl %myFLAGS% yassl.cpp
cl %myFLAGS% yassl_error.cpp

cl %myFLAGS% yassl_imp.cpp
cl %myFLAGS% yassl_int.cpp

link.exe -lib /out:yassl.lib buffer.obj cert_wrapper.obj crypto_wrapper.obj handshake.obj lock.obj log.obj socket_wrapper.obj ssl.obj template_instnt.obj timer.obj yassl.obj yassl_error.obj yassl_imp.obj yassl_int.obj



