TARGET = ndbapi_multi_cursor
SRCS = main.cpp
OBJS = main.o
##CXX = g++
CFLAGS = -c -g -m64 -fno-rtti -fno-exceptions
CXXFLAGS = 
DEBUG = 
LDFLAGS = -g -m64
TOP_SRCDIR = ../../../..
INCLUDE_DIR = $(TOP_SRCDIR)/storage/ndb/include
LIB_DIR = -L$(TOP_SRCDIR)/storage/ndb/src/.libs
SYS_LIB = 

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(LIB_DIR) $(OBJS) -lrt -lndbclient $(SYS_LIB) -o $(TARGET)

$(TARGET).quantify: $(OBJS)
	quantify $(CXX) $(CXXFLAGS) $(LDFLAGS) $(LIB_DIR) $(OBJS) -lrt -lndbclient $(SYS_LIB) -o $(TARGET)


$(OBJS): $(SRCS)
	$(CXX) $(CFLAGS) -I$(TOP_SRCDIR)/include -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/ndbapi $(SRCS)

clean:
	rm -f *.o $(TARGET)
