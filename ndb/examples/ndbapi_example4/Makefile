-include .defs.mk
#NDB_OS = OS_YOU_ARE_RUNNING_ON
#You need to set the NDB_OS variable here
TARGET = ndbapi_example4
SRCS = ndbapi_example4.cpp
OBJS = ndbapi_example4.o
CXX = g++
CFLAGS = -c -Wall -fno-rtti -fno-exceptions
DEBUG = 
LFLAGS = -Wall
INCLUDE_DIR = ../../include
LIB_DIR = ../../lib
ifeq ($(NDB_OS), SOLARIS)
# Here is the definition of system libraries necessary for Solaris 7
SYS_LIB = 
endif
ifeq ($(NDB_OS), LINUX)
# Here is the definition of system libraries necessary for Linux 2.4
SYS_LIB = 
endif
ifeq ($(NDB_OS), MACOSX)
# Here is the definition of system libraries necessary for Mac OS X
SYS_LIB = 
endif

$(TARGET): $(OBJS)
	$(CXX) $(LFLAGS) -L$(LIB_DIR) $(OBJS) -lNDB_API $(SYS_LIB) -o $(TARGET)

$(TARGET).o: $(SRCS)
	$(CXX) $(CFLAGS) -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/ndbapi $(SRCS)

clean:
	rm -f *.o $(TARGET)
