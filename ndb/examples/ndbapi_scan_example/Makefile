-include .defs.mk
#NDB_OS = OS_YOU_ARE_RUNNING_ON
#You need to set the NDB_OS variable here  (LINUX, SOLARIS, MACOSX)
#NDB_OS = LINUX

TARGET = ndbapi_scan
SRCS = ndbapi_scan.cpp 
OBJS = ndbapi_scan.o
CC = g++
CFLAGS = -c -Wall -fno-rtti
DEBUG = 
LFLAGS = -Wall
INCLUDE_DIR = ../../include
LIB_DIR = ../../lib
ifeq ($(NDB_OS), SOLARIS)
# Here is the definition of system libraries necessary for Solaris 7
SYS_LIB = -lpthread -lsocket -lnsl -lrt
endif
ifeq ($(NDB_OS), LINUX)
# Here is the definition of system libraries necessary for Linux 2.4
SYS_LIB = -lpthread
endif
ifeq ($(NDB_OS), MACOSX)
# Here is the definition of system libraries necessary for Mac OS X
SYS_LIB = 
endif

$(TARGET): $(OBJS)
	$(CC) $(LFLAGS) -L$(LIB_DIR) -lNDB_API $(OBJS) $(SYS_LIB) -o $(TARGET)

$(TARGET).o: $(SRCS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/ndbapi $(SRCS)

clean:
	rm -f *.o $(TARGET)
