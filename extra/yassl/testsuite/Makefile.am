# Copyright (c) 2006, 2010, Oracle and/or its affiliates. All rights reserved.
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
noinst_PROGRAMS       = testsuite
testsuite_SOURCES  = testsuite.cpp ../taocrypt/test/test.cpp \
	../examples/client/client.cpp ../examples/server/server.cpp \
	../examples/echoclient/echoclient.cpp \
	../examples/echoserver/echoserver.cpp
testsuite_CXXFLAGS = -DYASSL_PURE_C -DYASSL_PREFIX -DNO_MAIN_DRIVER @yassl_thread_cxxflags@
testsuite_LDADD    = $(top_builddir)/extra/yassl/src/libyassl.la \
                     $(top_builddir)/extra/yassl/taocrypt/src/libtaocrypt.la
EXTRA_DIST = testsuite.dsp test.hpp input quit make.bat

# Don't update the files from bitkeeper
%::SCCS/s.%

