# Copyright (c) 2010, 2012 Oracle and/or its affiliates. All rights reserved.
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

## all subdirectories that have been configured (have a generated Makefile)
DIST_SUBDIRS = myapi myjapi unload

## all subdirectories in DIST_SUBDIRS that should be built
SUBDIRS =  myapi myjapi unload

## files to be distributed and not covered in the automatic rules
EXTRA_DIST = CMakeLists.txt

## ----------------------------------------------------------------------

## automake flags (overridden by per-target flags, added before user flags)
# ndb71@win32: compile issues with including <NdbTap.hpp>
#AM_CPPFLAGS = -I$(top_srcdir)/storage/ndb/include/util \
#		-I$(top_srcdir)/storage/ndb/include
AM_CPPFLAGS = -I$(top_srcdir)/unittest/mytap

## build a noninstalled test program
noinst_PROGRAMS = jtie_unit_tests-t
#check_PROGRAMS = jtie_unit_tests-t

## source files to build the program (dist_ by default)
jtie_unit_tests_t_SOURCES = jtie_unit_tests-t.cpp

## per-target compilation flags (overrides the AM_ but not the user flags)
#jtie_unit_tests_t_CPPFLAGS = $(AM_CPPFLAGS)

## extra libtool objects (.lo) or libtool libraries (.la)
#jtie_unit_tests_t_LDADD = libmytap.la

## mode-specific libtool options (overrides AM_LDFLAGS)
##   using -no-install requires shared libraries to be installed
#jtie_unit_tests_t_LDFLAGS = -no-install ## never needs wrapper script
jtie_unit_tests_t_LDFLAGS = -no-undefined ## build self-contained executable
#jtie_unit_tests_t_LDFLAGS += -static ## only create a static executable

## ----------------------------------------------------------------------

## programs or scripts to run for testing
# unit-tests invoked individually in subdirectories
#TESTS = $(noinst_PROGRAMS)

## ----------------------------------------------------------------------

## cleanup
MOSTLYCLEANFILES = *.log
CLEANFILES =

## ----------------------------------------------------------------------
