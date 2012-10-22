# Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

INCLUDES = -I$(srcdir)/../include -I$(srcdir)/../mySTL

noinst_LTLIBRARIES = libtaocrypt.la

libtaocrypt_la_SOURCES  = aes.cpp aestables.cpp algebra.cpp arc4.cpp \
        asn.cpp bftables.cpp blowfish.cpp coding.cpp des.cpp dh.cpp \
        dsa.cpp file.cpp hash.cpp integer.cpp md2.cpp md4.cpp md5.cpp misc.cpp \
        random.cpp ripemd.cpp rsa.cpp sha.cpp template_instnt.cpp \
        tftables.cpp twofish.cpp crypto.cpp rabbit.cpp hc128.cpp

libtaocrypt_la_CXXFLAGS = @yassl_taocrypt_extra_cxxflags@ -DYASSL_PURE_C \
			  @yassl_thread_cxxflags@

EXTRA_DIST = $(wildcard ../include/*.hpp)

# Don't update the files from bitkeeper
%::SCCS/s.%

