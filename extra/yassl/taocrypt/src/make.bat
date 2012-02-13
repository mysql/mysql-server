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

set myFLAGS= /I../include /I../mySTL /c /W3 /G6 /O2 

cl %myFLAGS% aes.cpp
cl %myFLAGS% aestables.cpp
cl %myFLAGS% algebra.cpp
cl %myFLAGS% arc4.cpp

cl %myFLAGS% asn.cpp
cl %myFLAGS% bftables.cpp
cl %myFLAGS% blowfish.cpp
cl %myFLAGS% coding.cpp

cl %myFLAGS% des.cpp
cl %myFLAGS% dh.cpp
cl %myFLAGS% dsa.cpp
cl %myFLAGS% file.cpp

cl %myFLAGS% hash.cpp
cl %myFLAGS% integer.cpp
cl %myFLAGS% md2.cpp
cl %myFLAGS% md4.cpp
cl %myFLAGS% md5.cpp

cl %myFLAGS% misc.cpp
cl %myFLAGS% random.cpp
cl %myFLAGS% ripemd.cpp
cl %myFLAGS% rsa.cpp

cl %myFLAGS% sha.cpp
cl %myFLAGS% template_instnt.cpp
cl %myFLAGS% tftables.cpp
cl %myFLAGS% twofish.cpp

link.exe -lib /out:taocrypt.lib aes.obj aestables.obj algebra.obj arc4.obj asn.obj bftables.obj blowfish.obj coding.obj des.obj dh.obj dsa.obj file.obj hash.obj integer.obj md2.obj md4.obj md5.obj misc.obj random.obj ripemd.obj rsa.obj sha.obj template_instnt.obj tftables.obj twofish.obj

