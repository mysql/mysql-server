TARGET = ndbapi_example4
SRCS = ndbapi_example4.cpp
OBJS = ndbapi_example4.o
CXX = g++
CFLAGS = -c -Wall -fno-rtti -fno-exceptions
DEBUG = 
LFLAGS = -Wall
INCLUDE_DIR = ../../include
LIB_DIR = -L../../src/.libs \
          -L../../../libmysql_r/.libs \
          -L../../../mysys
SYS_LIB = 

$(TARGET): $(OBJS)
	$(CXX) $(LFLAGS) $(LIB_DIR) $(OBJS) -lndbclient -lmysqlclient_r -lmysys -lz $(SYS_LIB) -o $(TARGET)

$(TARGET).o: $(SRCS)
	$(CXX) $(CFLAGS) -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/ndbapi $(SRCS)

clean:
	rm -f *.o $(TARGET)
