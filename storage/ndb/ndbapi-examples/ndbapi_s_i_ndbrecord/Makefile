TARGET = ndbapi_simple_index_ndbrecord
SRCS = main.cpp
OBJS = main.o
CXX = g++
CFLAGS = -c -Wall -fno-rtti -fno-exceptions
CXXFLAGS = 
DEBUG = 
LFLAGS = -Wall
TOP_SRCDIR = ../../../..
INCLUDE_DIR = $(TOP_SRCDIR)
LIB_DIR = -L$(TOP_SRCDIR)/storage/ndb/src/.libs \
          -L$(TOP_SRCDIR)/libmysql_r/.libs \
          -L$(TOP_SRCDIR)/zlib/.libs \
          -L$(TOP_SRCDIR)/mysys -L$(TOP_SRCDIR)/strings
SYS_LIB = 

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LFLAGS) $(LIB_DIR) $(OBJS) -lndbclient -lmysqlclient_r -lmysys -lmystrings -lz $(SYS_LIB) -o $(TARGET)

$(OBJS): $(SRCS)
	$(CXX) $(CFLAGS) -I$(INCLUDE_DIR)/include -I$(INCLUDE_DIR)/storage/ndb/include -I$(INCLUDE_DIR)/storage/ndb/include/ndbapi $(SRCS)

clean:
	rm -f *.o $(TARGET)
