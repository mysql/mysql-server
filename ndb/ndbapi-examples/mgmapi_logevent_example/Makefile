TARGET = mgmapi_logevent
SRCS = $(TARGET).cpp
OBJS = $(TARGET).o
CXX = g++
CFLAGS = -c -Wall -fno-rtti -fno-exceptions
CXXFLAGS = 
DEBUG = 
LFLAGS = -Wall
TOP_SRCDIR = ../../..
INCLUDE_DIR = $(TOP_SRCDIR)
LIB_DIR = -L$(TOP_SRCDIR)/ndb/src/.libs \
          -L$(TOP_SRCDIR)/libmysql_r/.libs \
          -L$(TOP_SRCDIR)/mysys
SYS_LIB = 

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LFLAGS) $(LIB_DIR) $(OBJS) -lndbclient -lmysqlclient_r -lmysys -lz $(SYS_LIB) -o $(TARGET)

$(TARGET).o: $(SRCS)
	$(CXX) $(CFLAGS) -I$(INCLUDE_DIR)/include -I$(INCLUDE_DIR)/ndb/include -I$(INCLUDE_DIR)/ndb/include/mgmapi  -I$(INCLUDE_DIR)/ndb/include/ndbapi $(SRCS)

clean:
	rm -f *.o $(TARGET)
