TARGET = ndbapi_example5
SRCS = ndbapi_example5.cpp
OBJS = ndbapi_example5.o
CXX = g++
CFLAGS = -c -Wall -fno-rtti -fno-exceptions
CXXFLAGS = 
DEBUG = 
LFLAGS = -Wall
TOP_SRCDIR = ../../..
INCLUDE_DIR = $(TOP_SRCDIR)/ndb/include
LIB_DIR = -L$(TOP_SRCDIR)/ndb/src/.libs \
          -L$(TOP_SRCDIR)/libmysql/.libs \
          -L$(TOP_SRCDIR)/mysys
SYS_LIB = 

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(LFLAGS) $(LIB_DIR) $(OBJS) -lndbclient -lmysqlclient -lmysys $(SYS_LIB) -o $(TARGET)

$(TARGET).o: $(SRCS)
	$(CXX) $(CFLAGS) -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/ndbapi $(SRCS)

clean:
	rm -f *.o $(TARGET)
