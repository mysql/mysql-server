TARGET = ndbapi_retries
SRCS = ndbapi_retries.cpp
OBJS = ndbapi_retries.o
CXX = g++
CFLAGS = -c -Wall -fno-rtti -fno-exceptions
DEBUG = 
LFLAGS = -Wall
TOP_SRCDIR = ../../../..
INCLUDE_DIR = ../../include
LIB_DIR = -L$(TOP_SRCDIR)/storage/ndb/src/.libs \
          -L$(TOP_SRCDIR)/libmysql_r/.libs \
          -L$(TOP_SRCDIR)/mysys -L$(TOP_SRCDIR)/strings
SYS_LIB = 

$(TARGET): $(OBJS)
	$(CXX) $(LFLAGS) $(LIB_DIR) $(OBJS) -lndbclient -lmysqlclient_r -lmysys -lmystrings -lz $(SYS_LIB) -o $(TARGET)

$(TARGET).o: $(SRCS)
	$(CXX) $(CFLAGS) -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/ndbapi $(SRCS)

clean:
	rm -f *.o $(TARGET)
