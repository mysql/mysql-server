# Copyright (C) 2005, 2006 MySQL AB, 2008 Sun Microsystems, Inc.
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

TARGET = ndbapi_event
SRCS = ndbapi_event.cpp
OBJS = ndbapi_event.o
CXX = g++ -g
CFLAGS = -c -Wall -fno-rtti -fno-exceptions
CXXFLAGS = 
DEBUG =# -DVM_TRACE
LFLAGS = -Wall
TOP_SRCDIR = ../../../..
INCLUDE_DIR = $(TOP_SRCDIR)/storage/ndb/include
LIB_DIR = -L$(TOP_SRCDIR)/storage/ndb/src/.libs \
          -L$(TOP_SRCDIR)/libmysql_r/.libs
SYS_LIB = 

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LFLAGS) $(LIB_DIR) $(OBJS) -lndbclient -lmysqlclient_r $(SYS_LIB) -o $(TARGET)

$(TARGET).o: $(SRCS) Makefile
	$(CXX) $(CFLAGS) $(DEBUG) -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/ndbapi -I$(TOP_SRCDIR)/include $(SRCS)

clean:
	rm -f *.o $(TARGET)
