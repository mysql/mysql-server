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
          -L$(TOP_SRCDIR)/libmysql_r/.libs \
          -L$(TOP_SRCDIR)/zlib/.libs \
          -L$(TOP_SRCDIR)/mysys -L$(TOP_SRCDIR)/strings
SYS_LIB = 

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LFLAGS) $(LIB_DIR) $(OBJS) -lndbclient -lmysqlclient_r -lmysys -lmystrings -lz $(SYS_LIB) -o $(TARGET)

$(TARGET).o: $(SRCS) Makefile
	$(CXX) $(CFLAGS) $(DEBUG) -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/ndbapi -I$(TOP_SRCDIR)/include $(SRCS)

clean:
	rm -f *.o $(TARGET)
