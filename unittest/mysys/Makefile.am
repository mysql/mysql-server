# Copyright (C) 2009 Sun Microsystems, Inc.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

AM_CPPFLAGS      = @ZLIB_INCLUDES@ -I$(top_builddir)/include 
AM_CPPFLAGS     += -I$(top_srcdir)/include -I$(top_srcdir)/unittest/mytap

noinst_HEADERS = thr_template.c

LDADD 		= $(top_builddir)/unittest/mytap/libmytap.a \
		  $(top_builddir)/mysys/libmysys.a \
		  $(top_builddir)/dbug/libdbug.a \
		  $(top_builddir)/strings/libmystrings.a

noinst_PROGRAMS  = bitmap-t base64-t lf-t my_rdtsc-t my_vsnprintf-t

if NEED_THREAD
# my_atomic-t is used to check thread functions, so it is safe to 
# ignore the file in non-threaded builds.
# In fact, it will not compile without thread support.
noinst_PROGRAMS += my_atomic-t
endif

EXTRA_DIST       = CMakeLists.txt
