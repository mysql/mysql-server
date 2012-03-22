# Copyright (C) 2003, 2005 MySQL AB
#  All rights reserved. Use is subject to license terms.
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

include .defs.mk

TYPE := kernel

BIN_TARGET := aftest
BIN_TARGET_ARCHIVES := ndbfs portlib trace signaldataprint

SOURCES = AsyncFileTest.cpp

CFLAGS_AsyncFileTest.cpp = -I../

include $(NDB_TOP)/Epilogue.mk


# run basic tests
run_test :
	$(NDB_TOP)/bin/$(BIN_TARGET)
	$(NDB_TOP)/bin/$(BIN_TARGET) -n 8 -r 8 -l 10 -remove
	$(NDB_TOP)/bin/$(BIN_TARGET) -n 8 -r 8 -l 10 -reverse -remove
	$(NDB_TOP)/bin/$(BIN_TARGET) -n 8 -r 8 -l 10 -s 512 -remove
	$(NDB_TOP)/bin/$(BIN_TARGET) -n 8 -r 4 -l 1000






