# Copyright (C) 2009 Sun Microsystems, Inc.
# All rights reserved. Use is subject to license terms.
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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

include $(top_srcdir)/storage/ndb/config/common.mk.am

## ----------------------------------------------------------------------

## automake flags (overridden by per-target flags, added before user flags)
AM_CPPFLAGS = -I../include -I$(top_srcdir)/include \
		-I$(top_srcdir)/storage/ndb/include \
		-I$(top_srcdir)/storage/ndb/include/util \
		-I$(top_srcdir)/storage/ndb/include/portlib

## build a libtool convenience (i.e., non-installed) library
##   prefixes check_, noinst_ result in no -rpath option being passed,
##   which prevents libtool from creating a shared library.
noinst_LTLIBRARIES = libmysqlutils.la
#check_LTLIBRARIES = libmysqlutils.la

## source files to build the library (added to the distribution by default)
libmysqlutils_la_SOURCES = \
		decimal_utils.cpp \
		CharsetMap.cpp \
		CharsetMapImpl.cpp

## per-target compilation flags (overrides the AM_ but not the user flags)
#libmysqlutils_la_CPPFLAGS = $(AM_CPPFLAGS)

## extra libtool objects (.lo) or libtool libraries (.la)
##   while unsatisfactory to include the headers from one lib but link
##   against another one that happens to also define the symbols, we
##   do not want to add another lib dependency here
#libmysqlutils_la_LIBADD = $(top_srcdir)/libmysql/libmysqlclient.la
libmysqlutils_la_LIBADD = $(top_srcdir)/storage/ndb/src/libndbclient.la
##   linking directly against a static (and not a libtool) library to ensure
##   self-containment violates libtool conventions and triggers a warning
##   about non-portability -- possibly, because of a potential pic/non-pic
##   code mix on some platforms.
#libmysqlutils_la_LIBADD = $(top_srcdir)/storage/ndb/src/.libs/libndbclient.a

## mode-specific libtool options (overrides AM_LDFLAGS)
libmysqlutils_la_LDFLAGS = -no-undefined ## build self-contained library
#libmysqlutils_la_LDFLAGS += -shared ## only create a shared library
#libmysqlutils_la_LDFLAGS += -static ## only create a static library

## generic libtool options (overrides AM_LIBTOOLFLAGS)
#libmysqlutils_la_LIBTOOLFLAGS = -lstdc++

## ----------------------------------------------------------------------

## cleanup
#MOSTLYCLEANFILES =
#CLEANFILES =

## ----------------------------------------------------------------------
