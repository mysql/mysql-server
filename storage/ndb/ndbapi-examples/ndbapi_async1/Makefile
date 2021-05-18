# Copyright (C) 2005, 2006 MySQL AB, 2008 Sun Microsystems, Inc.
#  All rights reserved. Use is subject to license terms.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

TARGET = ndbapi_async1
SRCS = ndbapi_async1.cpp
OBJS = ndbapi_async1.o
CXX = g++
CFLAGS = -c -Wall -fno-rtti -fno-exceptions
DEBUG = 
LFLAGS = -Wall
TOP_SRCDIR = ../../../..
INCLUDE_DIR = $(TOP_SRCDIR)/storage/ndb/include
LIB_DIR = -L$(TOP_SRCDIR)/storage/ndb/src \
          -L$(TOP_SRCDIR)/libmysql
SYS_LIB = 

$(TARGET): $(OBJS)
	$(CXX) $(LFLAGS) $(LIB_DIR) $(OBJS) -lndbclient -lmysqlclient_r $(SYS_LIB) -o $(TARGET)

$(TARGET).o: $(SRCS)
	$(CXX) $(CFLAGS) -I$(TOP_SRCDIR)/include -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/ndbapi $(SRCS)

clean:
	rm -f *.o $(TARGET)
