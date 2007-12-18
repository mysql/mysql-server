CPPFLAGS = -I../include
CXXFLAGS = -Wall -g
CC = c++
LDFLAGS = -lz
SRCS = $(wildcard *.cpp)
OBJS = $(patsubst %.cpp, %.o, $(SRCS))
default: $(OBJS)
$(OBJS): db_cxx.h
test1: test1.o dbt.o db.o dbenv.o ../lib/libdb.a

