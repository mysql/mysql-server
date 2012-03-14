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

## build a noninstalled test program
noinst_PROGRAMS = jtie_unit_tests-t
jtie_unit_tests_t_SOURCES = jtie_unit_tests-t.cpp
jtie_unit_tests_t_LDFLAGS = -no-undefined ## build self-contained executable
jtie_unit_tests_t_CXXFLAGS = -I$(top_srcdir)/storage/ndb/include

## ----------------------------------------------------------------------

## cleanup
MOSTLYCLEANFILES = *.log
CLEANFILES =

## ----------------------------------------------------------------------
