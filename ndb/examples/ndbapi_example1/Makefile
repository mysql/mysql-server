TARGET = ndbapi_example1
SRCS = ndbapi_example1.cpp
OBJS = ndbapi_example1.o
CXX = g++
CFLAGS = -c -Wall -fno-rtti -fno-exceptions
DEBUG = 
LFLAGS = -Wall
INCLUDE_DIR = ../../include
LIB_DIR = -L../../src/.libs \
          -L../../../libmysql/.libs \
          -L../../../mysys
SYS_LIB = 

$(TARGET): $(OBJS)
	$(CXX) $(LFLAGS) $(LIB_DIR) $(OBJS) -lndbclient -lmysqlclient -lmysys $(SYS_LIB) -o $(TARGET)

$(TARGET).o: $(SRCS)
	$(CXX) $(CFLAGS) -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/ndbapi $(SRCS)

clean:
	rm -f *.o $(TARGET)
