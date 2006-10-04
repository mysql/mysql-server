TARGET = mgmapi_logevent_dual
SRCS = $(TARGET).cpp
OBJS = $(TARGET).o
CXX = g++
CFLAGS = -c -Wall -fno-rtti -fno-exceptions
CXXFLAGS = 
DEBUG = 
LFLAGS = -Wall
TOP_SRCDIR = ../../../..
INCLUDE_DIR = $(TOP_SRCDIR)/storage/ndb/include
LIB_DIR = -L$(TOP_SRCDIR)/storage/ndb/src/.libs \
          -L$(TOP_SRCDIR)/libmysql_r/.libs \
	  -L$(TOP_SRCDIR)/zlib/.libs \
          -L$(TOP_SRCDIR)/mysys -L$(TOP_SRCDIR)/strings
SYS_LIB = 

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LFLAGS) $(LIB_DIR) $(OBJS) -lndbclient -lmysqlclient_r -lmysys -lmystrings -lz $(SYS_LIB) -o $(TARGET)

$(TARGET).o: $(SRCS)
	$(CXX) $(CFLAGS) -I$(TOP_SRCDIR)/include -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/mgmapi  -I$(INCLUDE_DIR)/ndbapi $(SRCS)

clean:
	rm -f *.o $(TARGET)
