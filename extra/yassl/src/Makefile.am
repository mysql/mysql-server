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

INCLUDES = -I$(srcdir)/../include -I$(srcdir)/../taocrypt/include -I$(srcdir)/../taocrypt/mySTL

noinst_LTLIBRARIES = libyassl.la
libyassl_la_SOURCES = buffer.cpp cert_wrapper.cpp crypto_wrapper.cpp \
	handshake.cpp lock.cpp log.cpp socket_wrapper.cpp ssl.cpp \
	template_instnt.cpp timer.cpp yassl_imp.cpp yassl_error.cpp yassl_int.cpp
EXTRA_DIST = $(wildcard ../include/*.hpp) $(wildcard ../include/openssl/*.h)
AM_CXXFLAGS = -DYASSL_PURE_C -DYASSL_PREFIX @yassl_thread_cxxflags@

# Don't update the files from bitkeeper
%::SCCS/s.%

